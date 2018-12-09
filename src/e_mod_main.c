#include "e.h"
#include "../config.h"
/* module setup */
E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
     "Presentator helper tool"
};

static Evas_Object *notify, *lb;
static Eina_Hash *string_hash;
static Ecore_Event_Handler *event_data, *event_error;
static Ecore_Exe *keylogger;

typedef struct {
   unsigned int ref;
   char *name;
   Ecore_Timer *timer;
} Entry;

typedef struct {
   float scale;
} Presentator_Config;

static E_Config_DD *dd_config;
static Presentator_Config *config;

static void _del_key(const char *name);

static Eina_Bool
_timed_del(void *data)
{
   Entry *e = data;
   _del_key(e->name);
   e->timer = NULL;
   return 0;
}

static void
_update_place(void)
{
   E_Zone *zone = e_zone_current_get();
   Eina_Rectangle geom, notify_geom;
   Evas_Coord minw = -1, minh = -1;
   Edje_Object *edje;
   Eina_Strbuf *buf;
   Entry *e;

   Eina_Iterator* iter = eina_hash_iterator_data_new(string_hash);

   buf = eina_strbuf_new();

   EINA_ITERATOR_FOREACH(iter, e)
     {
        if (!!eina_strbuf_length_get(buf))
          eina_strbuf_append(buf, " + ");
        eina_strbuf_append(buf, e->name);
     }

   eina_iterator_free(iter);
   iter = NULL;

   if (!eina_strbuf_length_get(buf))
     evas_object_hide(notify);
   else
     evas_object_show(notify);

   elm_object_text_set(lb, eina_strbuf_string_get(buf));

   eina_strbuf_free(buf);

   edje = elm_layout_edje_get(notify);
   edje_object_size_min_calc(edje, &minw, &minh);

   e_zone_useful_geometry_get(zone, &geom.x, &geom.y, &geom.w, &geom.h);
   notify_geom.x = geom.x + (geom.w - minw) /2;
   notify_geom.y = geom.y + geom.h - minh;
   notify_geom.w = minw;
   notify_geom.h = minh;

   evas_object_geometry_set(notify, EINA_RECTANGLE_ARGS(&notify_geom));
}

static void
_flush_config(void)
{
   e_config_domain_save("presentator", dd_config, config);
   elm_object_scale_set(lb, config->scale);
   _update_place();
}

static void
_del_key(const char *name)
{
   Entry *e = eina_hash_find(string_hash, name);

   //something fishy is going on sometimes, then we dont even have a entry
   if (!e)
     {
        /* there was a release without a hit, usally happens when we are starting up */
        return;
     }

   //deref the entry
   e->ref --;

   if (e->ref == 1)
     {
        if (e->timer) ecore_timer_del(e->timer);
        e->timer = ecore_timer_add(0.5, _timed_del, e);
     }

   //free if null
   if (e->ref == 0)
     {

        //if ref is 0 nuke out that entry
        eina_hash_del_by_key(string_hash, name);
        //we need to update the notify visuals depending on the state of the box
        _update_place();
     }
}

static void
_add_key(const char *name)
{
   Entry *e = eina_hash_find(string_hash, name);

   //first if we have already displayed that string do not display it again
   if (e)
     {
        e->ref ++;
        E_FREE_FUNC(e->timer, ecore_timer_del);
        return;
     }

   //save as entry somewhere
   e = calloc(1, sizeof(Entry));
   e->ref = 2; //one for beeing used and one for the minimum time
   e->name = strdup(name);
   eina_hash_add(string_hash, name, e);

   //update the place here
   _update_place();
}

static void
_entry_free(Entry *e)
{
   free(e->name);
   free(e);
}

static Eina_Bool
_msg_from_child_handler(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Data *dataFromProcess = (Ecore_Exe_Event_Data *)event;

   for (int i = 0; dataFromProcess->lines[i].line; ++i)
     {
        char first = dataFromProcess->lines[i].line[0];
        char *key = &dataFromProcess->lines[i].line[1];
        if (first == ',')
          {
             _del_key(key);
          }
        else if (first == '.')
          {
             _add_key(key);
          }
        else
        {
            ERR("Unknown message from the keylogger");
        }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_msg_from_child_handler_error(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   ERR("The keylogger passed out");

   return ECORE_CALLBACK_PASS_ON;
}


static void
_start_logging(void)
{
   string_hash = eina_hash_string_small_new((Eina_Free_Cb)_entry_free);

   notify = elm_layout_add(e_comp->elm);
   elm_layout_theme_set(notify, "notify", "bottom", "default");
   evas_object_layer_set(notify, E_LAYER_POPUP);
   evas_object_pass_events_set(notify, EINA_TRUE);

   lb = elm_label_add(notify);
   elm_object_content_set(notify, lb);
   evas_object_show(lb);


   _flush_config();
   /* FIXME this is only on x11 need a different solution for wl */
   {
      keylogger = ecore_exe_pipe_run(MODULE_DIR"/keylogger", ECORE_EXE_PIPE_WRITE |
                                                                 ECORE_EXE_PIPE_READ_LINE_BUFFERED |
                                                                 ECORE_EXE_PIPE_READ, notify);
      EINA_SAFETY_ON_NULL_RETURN(keylogger);
      event_data = ecore_event_handler_add(ECORE_EXE_EVENT_DATA, _msg_from_child_handler, NULL);
      event_error = ecore_event_handler_add(ECORE_EXE_EVENT_ERROR, _msg_from_child_handler_error, NULL);
   }
}

static void
_stop_logging(void)
{
   E_FREE_FUNC(keylogger, ecore_exe_kill);
   E_FREE_FUNC(notify, evas_object_del);
   E_FREE_FUNC(event_data, ecore_event_handler_del);
   E_FREE_FUNC(event_error, ecore_event_handler_del);
}

static E_Action *start_stop;
static Eina_Bool currently_running = EINA_FALSE;

static void
_toggle_run_mode(E_Object *e, const char *name)
{
   if (currently_running)
     {
        _stop_logging();
        currently_running = EINA_FALSE;
     }
   else
     {
        _start_logging();
        currently_running = EINA_TRUE;
     }
}

static void
_decrease_fontsize(E_Object *e, const char *name)
{
   float scale = elm_object_scale_get(lb);

   config->scale = scale - 0.1;
   _flush_config();
}

static void
_increase_fontsize(E_Object *e, const char *name)
{
   float scale = elm_object_scale_get(lb);

   config->scale = scale + 0.1;
   _flush_config();
}

static void
_config_init(void)
{
   E_Config_DD *result = E_CONFIG_DD_NEW("presentator", Presentator_Config);

   E_CONFIG_VAL(result, Presentator_Config, scale, FLOAT);

   dd_config = result;
}

E_API void *
e_modapi_init(E_Module *m)
{
   ecore_init();
   _config_init();

   config = e_config_domain_load("presentator", dd_config);

   if (!config)
     config = calloc(1, sizeof(Presentator_Config));

#define ACTION_ADD(_action, _cb, _title, _value, _params, _example, _editable) \
  {                                                                            \
     const char *_name = _value;                                               \
     if ((_action = e_action_add(_name))) {                                    \
          _action->func.go = _cb;                                              \
          e_action_predef_name_set("Presentator", _title, _name,                \
                                   _params, _example, _editable);              \
       }                                                                       \
  }
  ACTION_ADD(start_stop, _toggle_run_mode, "Start/Stop", "Toggle key highlight mode", NULL, NULL, EINA_FALSE);
  ACTION_ADD(start_stop, _increase_fontsize, "Increase Fontsize", "Increase Fontsize", NULL, NULL, EINA_FALSE);
  ACTION_ADD(start_stop, _decrease_fontsize, "Decrease Fontsize", "Decrease Fontsize", NULL, NULL, EINA_FALSE);
#undef ACTION_ADD

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   E_CONFIG_DD_FREE(dd_config);
   free(config);
   config = NULL;

   ecore_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   return 1;
}
