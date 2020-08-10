/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2014 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Marco van Wieringen, August 2012
 */
/**
 * @file
 * Python Storage daemon Plugin program
 */

#if defined(HAVE_WIN32)
#include "include/bareos.h"
#include <Python.h>
#else
#include <Python.h>
#include "include/bareos.h"
#endif
#include "stored/stored.h"

#if (PY_VERSION_HEX < 0x02060000)
#error "Need at least Python version 2.6 or newer"
#endif

#include "python-sd.h"
#include "lib/edit.h"

namespace storagedaemon {

static const int debuglevel = 150;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Marco van Wieringen"
#define PLUGIN_DATE "October 2013"
#define PLUGIN_VERSION "3"
#define PLUGIN_DESCRIPTION "Python Storage Daemon Plugin"
#define PLUGIN_USAGE                                                           \
  "python:instance=<instance_id>:module_path=<path-to-python-modules>:module_" \
  "name=<python-module-to-load>"


/* Forward referenced functions */
static bRC newPlugin(bpContext* bareos_plugin_ctx);
static bRC freePlugin(bpContext* bareos_plugin_ctx);
static bRC getPluginValue(bpContext* bareos_plugin_ctx,
                          pVariable var,
                          void* value);
static bRC setPluginValue(bpContext* bareos_plugin_ctx,
                          pVariable var,
                          void* value);
static bRC handlePluginEvent(bpContext* bareos_plugin_ctx,
                             bsdEvent* event,
                             void* value);
static bRC parse_plugin_definition(bpContext* bareos_plugin_ctx,
                                   void* value,
                                   PoolMem& plugin_options);

static void PyErrorHandler(bpContext* bareos_plugin_ctx, int msgtype);
static bRC PyLoadModule(bpContext* bareos_plugin_ctx, void* value);
static bRC PyParsePluginDefinition(bpContext* bareos_plugin_ctx, void* value);
static bRC PyGetPluginValue(bpContext* bareos_plugin_ctx,
                            pVariable var,
                            void* value);
static bRC PySetPluginValue(bpContext* bareos_plugin_ctx,
                            pVariable var,
                            void* value);
static bRC PyHandlePluginEvent(bpContext* bareos_plugin_ctx,
                               bsdEvent* event,
                               void* value);

/* Pointers to Bareos functions */
static bsdFuncs* bfuncs = NULL;
static bsdInfo* binfo = NULL;

static genpInfo pluginInfo = {sizeof(pluginInfo), SD_PLUGIN_INTERFACE_VERSION,
                              SD_PLUGIN_MAGIC,    PLUGIN_LICENSE,
                              PLUGIN_AUTHOR,      PLUGIN_DATE,
                              PLUGIN_VERSION,     PLUGIN_DESCRIPTION,
                              PLUGIN_USAGE};

static psdFuncs pluginFuncs = {sizeof(pluginFuncs), SD_PLUGIN_INTERFACE_VERSION,

                               /* Entry points into plugin */
                               newPlugin,  /* new plugin instance */
                               freePlugin, /* free plugin instance */
                               getPluginValue, setPluginValue,
                               handlePluginEvent};

/**
 * Plugin private context
 */
struct plugin_private_context {
  int64_t instance;     /* Instance number of plugin */
  bool python_loaded;   /* Plugin has python module loaded ? */
  bool python_path_set; /* Python plugin search path is set ? */
  char* module_path;    /* Plugin Module Path */
  char* module_name;    /* Plugin Module Name */
  PyThreadState*
      interpreter;   /* Python interpreter for this instance of the plugin */
  PyObject* pModule; /* Python Module entry point */
  PyObject* pyModuleFunctionsDict; /* Python Dictionary */
};

/**
 * We don't actually use this but we need it to tear down the
 * final python interpreter on unload of the plugin. Each instance of
 * the plugin get its own interpreter.
 */
static PyThreadState* mainThreadState;

/* functions common to all plugins */
#include "plugins/python_plugins_common.inc"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bareos can directly call these two entry points
 *  they are common to all Bareos plugins.
 *
 * External entry point called by Bareos to "load" the plugin
 */
bRC loadPlugin(bsdInfo* lbinfo,
               bsdFuncs* lbfuncs,
               genpInfo** pinfo,
               psdFuncs** pfuncs)
{
  bfuncs = lbfuncs; /* Set Bareos funct pointers */
  binfo = lbinfo;

  *pinfo = &pluginInfo;   /* Return pointer to our info */
  *pfuncs = &pluginFuncs; /* Return pointer to our functions */

  /* Setup Python */
#if PY_MAJOR_VERSION >= 3
  PyImport_AppendInittab("bareossd", &PyInit_bareossd);
#else
  PyImport_AppendInittab("bareossd", initbareossd);
#endif
  Py_InitializeEx(0);
  PyEval_InitThreads();
  mainThreadState = PyEval_SaveThread();

  return bRC_OK;
}

/**
 * External entry point to unload the plugin
 */
bRC unloadPlugin()
{
  /*
   * Terminate Python
   */
  PyEval_RestoreThread(mainThreadState);
  Py_Finalize();

  return bRC_OK;
}

#ifdef __cplusplus
}
#endif

/* Create a new instance of the plugin i.e. allocate our private storage */
static bRC newPlugin(bpContext* bareos_plugin_ctx)
{
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)malloc(
          sizeof(struct plugin_private_context));
  if (!plugin_priv_ctx) { return bRC_Error; }
  memset(plugin_priv_ctx, 0, sizeof(struct plugin_private_context));
  bareos_plugin_ctx->pContext =
      (void*)plugin_priv_ctx; /* set our context pointer */

  /* For each plugin instance we instantiate a new Python interpreter. */
  PyEval_AcquireThread(mainThreadState);
  plugin_priv_ctx->interpreter = Py_NewInterpreter();
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

  /*
   * Always register some events the python plugin itself can register
   * any other events it is interested in.
   */
  bfuncs->registerBareosEvents(bareos_plugin_ctx, 1, bsdEventNewPluginOptions);

  return bRC_OK;
}

/* Free a plugin instance, i.e. release our private storage */
static bRC freePlugin(bpContext* bareos_plugin_ctx)
{
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)bareos_plugin_ctx->pContext;

  if (!plugin_priv_ctx) { return bRC_Error; }

  /*
   * Stop any sub interpreter started per plugin instance.
   */
  PyEval_AcquireThread(plugin_priv_ctx->interpreter);


  if (plugin_priv_ctx->pModule) { Py_DECREF(plugin_priv_ctx->pModule); }

  Py_EndInterpreter(plugin_priv_ctx->interpreter);
  PyEval_ReleaseLock();

  free(plugin_priv_ctx);
  bareos_plugin_ctx->pContext = NULL;

  return bRC_OK;
}


static bRC handlePluginEvent(bpContext* bareos_plugin_ctx,
                             bsdEvent* event,
                             void* value)
{
  bRC retval = bRC_Error;
  bool event_dispatched = false;
  PoolMem plugin_options(PM_FNAME);
  plugin_private_context* plugin_priv_ctx =
      (plugin_private_context*)bareos_plugin_ctx->pContext;

  if (!plugin_priv_ctx) { goto bail_out; }

  /*
   * First handle some events internally before calling python if it
   * want to do some special handling on the event triggered.
   */
  switch (event->eventType) {
    case bsdEventNewPluginOptions:
      event_dispatched = true;
      retval =
          parse_plugin_definition(bareos_plugin_ctx, value, plugin_options);
      break;
    default:
      break;
  }

  /*
   * See if we have been triggered in the previous switch if not we have to
   * always dispatch the event. If we already processed the event internally
   * we only do a dispatch to the python entry point when that internal
   * processing was successful (e.g. retval == bRC_OK).
   */
  if (!event_dispatched || retval == bRC_OK) {
    PyEval_AcquireThread(plugin_priv_ctx->interpreter);

    /*
     * Now dispatch the event to Python.
     * First the calls that need special handling.
     */
    switch (event->eventType) {
      case bsdEventNewPluginOptions:
        /*
         * See if we already loaded the Python modules.
         */
        if (!plugin_priv_ctx->python_loaded) {
          retval = PyLoadModule(bareos_plugin_ctx, plugin_options.c_str());
        }

        /* Only try to call when the loading succeeded. */
        if (retval == bRC_OK) {
          retval = PyParsePluginDefinition(bareos_plugin_ctx,
                                           plugin_options.c_str());
        }
        break;
      default:
        /*
         * Handle the generic events e.g. the ones which are just passed on.
         * We only try to call Python when we loaded the right module until
         * that time we pretend the call succeeded.
         */
        if (plugin_priv_ctx->python_loaded) {
          retval = PyHandlePluginEvent(bareos_plugin_ctx, event, value);
        } else {
          retval = bRC_OK;
        }
        break;
    }

    PyEval_ReleaseThread(plugin_priv_ctx->interpreter);
  }

bail_out:
  return retval;
}

/**
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * python:module_path=<path>:module_name=<python_module_name>:...
 */
static bRC parse_plugin_definition(bpContext* bareos_plugin_ctx,
                                   void* value,
                                   PoolMem& plugin_options)
{
  bool found;
  int i, cnt;
  PoolMem plugin_definition(PM_FNAME);
  char *bp, *argument, *argument_value;
  plugin_private_context* plugin_priv_ctx =
      (plugin_private_context*)bareos_plugin_ctx->pContext;

  if (!value) { return bRC_Error; }

  /*
   * Parse the plugin definition.
   * Make a private copy of the whole string.
   */
  PmStrcpy(plugin_definition, (char*)value);

  bp = strchr(plugin_definition.c_str(), ':');
  if (!bp) {
    Jmsg(bareos_plugin_ctx, M_FATAL,
         "python-sd: Illegal plugin definition %s\n",
         plugin_definition.c_str());
    Dmsg(bareos_plugin_ctx, debuglevel,
         "python-sd: Illegal plugin definition %s\n",
         plugin_definition.c_str());
    goto bail_out;
  }

  /*
   * Skip the first ':'
   */
  bp++;

  cnt = 0;
  while (bp) {
    if (strlen(bp) == 0) { break; }

    /*
     * Each argument is in the form:
     *    <argument> = <argument_value>
     *
     * So we setup the right pointers here, argument to the beginning
     * of the argument, argument_value to the beginning of the argument_value.
     */
    argument = bp;
    argument_value = strchr(bp, '=');
    if (!argument_value) {
      Jmsg(bareos_plugin_ctx, M_FATAL,
           "python-sd: Illegal argument %s without value\n", argument);
      Dmsg(bareos_plugin_ctx, debuglevel,
           "python-sd: Illegal argument %s without value\n", argument);
      goto bail_out;
    }
    *argument_value++ = '\0';

    /*
     * See if there are more arguments and setup for the next run.
     */
    bp = argument_value;
    do {
      bp = strchr(bp, ':');
      if (bp) {
        if (*(bp - 1) != '\\') {
          *bp++ = '\0';
          break;
        } else {
          bp++;
        }
      }
    } while (bp);

    found = false;
    for (i = 0; plugin_arguments[i].name; i++) {
      if (Bstrcasecmp(argument, plugin_arguments[i].name)) {
        int64_t* int_destination = NULL;
        char** str_destination = NULL;
        bool* bool_destination = NULL;

        switch (plugin_arguments[i].type) {
          case argument_instance:
            int_destination = &plugin_priv_ctx->instance;
            break;
          case argument_module_path:
            str_destination = &plugin_priv_ctx->module_path;
            break;
          case argument_module_name:
            str_destination = &plugin_priv_ctx->module_name;
            break;
          default:
            break;
        }

        if (int_destination) {
          *int_destination = parse_integer(argument_value);
        }

        if (str_destination) { SetString(str_destination, argument_value); }

        if (bool_destination) {
          *bool_destination = ParseBoolean(argument_value);
        }

        /*
         * When we have a match break the loop.
         */
        found = true;
        break;
      }
    }

    /*
     * If we didn't consume this parameter we add it to the plugin_options list.
     */
    if (!found) {
      PoolMem option(PM_FNAME);

      if (cnt) {
        Mmsg(option, ":%s=%s", argument, argument_value);
        PmStrcat(plugin_options, option.c_str());
      } else {
        Mmsg(option, "%s=%s", argument, argument_value);
        PmStrcat(plugin_options, option.c_str());
      }
      cnt++;
    }
  }

  if (cnt > 0) { PmStrcat(plugin_options, ":"); }

  return bRC_OK;

bail_out:
  return bRC_Error;
}


/**
 * Initial load of the Python module.
 *
 * Based on the parsed plugin options we set some prerequisites like the
 * module path and the module to load. We also load the dictionary used
 * for looking up the Python methods.
 */
static bRC PyLoadModule(bpContext* bareos_plugin_ctx, void* value)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)bareos_plugin_ctx->pContext;
  PyObject *sysPath, *mPath, *pName, *pFunc;

  /*
   * See if we already setup the python search path.
   */
  if (!plugin_priv_ctx->python_path_set) {
    /*
     * Extend the Python search path with the given module_path.
     */
    if (plugin_priv_ctx->module_path) {
      sysPath = PySys_GetObject((char*)"path");
      mPath = PyString_FromString(plugin_priv_ctx->module_path);
      PyList_Append(sysPath, mPath);
      Py_DECREF(mPath);
      plugin_priv_ctx->python_path_set = true;
    }
  }

  /*
   * Try to load the Python module by name.
   */
  if (plugin_priv_ctx->module_name) {
    Dmsg(bareos_plugin_ctx, debuglevel,
         "python-sd: Trying to load module with name %s\n",
         plugin_priv_ctx->module_name);
    pName = PyString_FromString(plugin_priv_ctx->module_name);
    plugin_priv_ctx->pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (!plugin_priv_ctx->pModule) {
      Dmsg(bareos_plugin_ctx, debuglevel,
           "python-sd: Failed to load module with name %s\n",
           plugin_priv_ctx->module_name);
      goto bail_out;
    }

    Dmsg(bareos_plugin_ctx, debuglevel,
         "python-sd: Successfully loaded module with name %s\n",
         plugin_priv_ctx->module_name);

    /*
     * Get the Python dictionary for lookups in the Python namespace.
     */
    plugin_priv_ctx->pyModuleFunctionsDict =
        PyModule_GetDict(plugin_priv_ctx->pModule); /* Borrowed reference */

    StorePluginContextInPythonModule(bareos_plugin_ctx);

    /*
     * Lookup the load_bareos_plugin() function in the python module.
     */
    pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                                 "load_bareos_plugin"); /* Borrowed reference */
    if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pPluginDefinition, *pRetVal;

      pPluginDefinition = PyString_FromString((char*)value);
      if (!pPluginDefinition) { goto bail_out; }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, pPluginDefinition, NULL);
      Py_DECREF(pPluginDefinition);

      if (!pRetVal) {
        goto bail_out;
      } else {
        retval = ConvertPythonRetvalTobRCRetval(pRetVal);
        Py_DECREF(pRetVal);
      }
    } else {
      Dmsg(bareos_plugin_ctx, debuglevel,
           "python-sd: Failed to find function named load_bareos_plugins()\n");
      goto bail_out;
    }

    /*
     * Keep track we successfully loaded.
     */
    plugin_priv_ctx->python_loaded = true;
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(bareos_plugin_ctx, M_FATAL); }

  return retval;
}

/**
 * Any plugin options which are passed in are dispatched here to a Python method
 * and it can parse the plugin options. This function is also called after
 * PyLoadModule() has loaded the Python module and made sure things are
 * operational.
 */
static bRC PyParsePluginDefinition(bpContext* bareos_plugin_ctx, void* value)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)bareos_plugin_ctx->pContext;
  PyObject* pFunc;

  /*
   * Lookup the parse_plugin_definition() function in the python module.
   */
  pFunc =
      PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                           "parse_plugin_definition"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject *pPluginDefinition, *pRetVal;

    pPluginDefinition = PyString_FromString((char*)value);
    if (!pPluginDefinition) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pPluginDefinition, NULL);
    Py_DECREF(pPluginDefinition);

    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);
    }

    return retval;
  } else {
    Dmsg(
        bareos_plugin_ctx, debuglevel,
        "python-sd: Failed to find function named parse_plugin_definition()\n");
    return bRC_Error;
  }

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(bareos_plugin_ctx, M_FATAL); }

  return retval;
}

static bRC PyGetPluginValue(bpContext* bareos_plugin_ctx,
                            pVariable var,
                            void* value)
{
  return bRC_OK;
}

static bRC PySetPluginValue(bpContext* bareos_plugin_ctx,
                            pVariable var,
                            void* value)
{
  return bRC_OK;
}

static bRC PyHandlePluginEvent(bpContext* bareos_plugin_ctx,
                               bsdEvent* event,
                               void* value)
{
  bRC retval = bRC_Error;
  plugin_private_context* plugin_priv_ctx =
      (plugin_private_context*)bareos_plugin_ctx->pContext;
  PyObject* pFunc;

  /*
   * Lookup the handle_plugin_event() function in the python module.
   */
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "handle_plugin_event"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject *pEventType, *pRetVal;

    pEventType = PyInt_FromLong(event->eventType);

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pEventType, NULL);
    Py_DECREF(pEventType);

    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);
    }
  } else {
    Dmsg(bareos_plugin_ctx, debuglevel,
         "python-sd: Failed to find function named handle_plugin_event()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(bareos_plugin_ctx, M_FATAL); }

  return retval;
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to get certain internal values of the current Job.
 */
static PyObject* PyBareosGetValue(PyObject* self, PyObject* args)
{
  int var;
  bpContext* bareos_plugin_ctx = NULL;
  PyObject* pRetVal = NULL;

  if (!PyArg_ParseTuple(args, "i:BareosGetValue", &var)) { return NULL; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  switch (var) {
    case bsdVarJobId:
    case bsdVarLevel:
    case bsdVarType:
    case bsdVarJobStatus: {
      int value;

      bareos_plugin_ctx = GetPluginContextFromPythonModule();
      if (bfuncs->getBareosValue(bareos_plugin_ctx, (bsdrVariable)var,
                                 &value) == bRC_OK) {
        pRetVal = PyInt_FromLong(value);
      }
      break;
    }
    case bsdVarJobErrors:
    case bsdVarJobFiles:
    case bsdVarJobBytes: {
      uint64_t value = 0;

      bareos_plugin_ctx = GetPluginContextFromPythonModule();
      if (bfuncs->getBareosValue(bareos_plugin_ctx, (bsdrVariable)var,
                                 &value) == bRC_OK) {
        pRetVal = PyLong_FromUnsignedLong(value);
      }
      break;
    }
    case bsdVarJobName:
    case bsdVarJob:
    case bsdVarClient:
    case bsdVarPool:
    case bsdVarPoolType:
    case bsdVarStorage:
    case bsdVarMediaType:
    case bsdVarVolumeName: {
      char* value = NULL;

      bareos_plugin_ctx = GetPluginContextFromPythonModule();
      if (bfuncs->getBareosValue(bareos_plugin_ctx, (bsdrVariable)var,
                                 &value) == bRC_OK) {
        if (value) { pRetVal = PyString_FromString(value); }
      }
      break;
    }
    case bsdVarCompatible: {
      bool value;

      if (bfuncs->getBareosValue(NULL, (bsdrVariable)var, &value) == bRC_OK) {
        long bool_value;

        bool_value = (value) ? 1 : 0;
        pRetVal = PyBool_FromLong(bool_value);
      }
      break;
    }
    case bsdVarPluginDir: {
      char* value = NULL;

      if (bfuncs->getBareosValue(NULL, (bsdrVariable)var, &value) == bRC_OK) {
        if (value) { pRetVal = PyString_FromString(value); }
      }
      break;
    }
    default:
      bareos_plugin_ctx = GetPluginContextFromPythonModule();
      Dmsg(bareos_plugin_ctx, debuglevel,
           "python-sd: PyBareosGetValue unknown variable requested %d\n", var);
      break;
  }

  if (!pRetVal) {
    Py_INCREF(Py_None);
    pRetVal = Py_None;
  }

  return pRetVal;
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to get certain internal values of the current Job.
 */
static PyObject* PyBareosSetValue(PyObject* self, PyObject* args)
{
  int var;
  bpContext* bareos_plugin_ctx = NULL;
  bRC retval = bRC_Error;
  PyObject* pyValue;

  if (!PyArg_ParseTuple(args, "iO:BareosSetValue", &var, &pyValue)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  switch (var) {
    case bsdwVarVolumeName: {
      char* value;

      bareos_plugin_ctx = GetPluginContextFromPythonModule();
      value = PyString_AsString(pyValue);
      if (value) {
        bfuncs->setBareosValue(bareos_plugin_ctx, (bsdwVariable)var, value);
      }

      break;
    }
    case bsdwVarPriority:
    case bsdwVarJobLevel: {
      int value;

      bareos_plugin_ctx = GetPluginContextFromPythonModule();
      value = PyInt_AsLong(pyValue);
      if (value >= 0) {
        retval = bfuncs->setBareosValue(bareos_plugin_ctx, (bsdwVariable)var,
                                        &value);
      }
      break;
    }
    default:
      bareos_plugin_ctx = GetPluginContextFromPythonModule();
      Dmsg(bareos_plugin_ctx, debuglevel,
           "python-sd: PyBareosSetValue unknown variable requested %d\n", var);
      break;
  }

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to issue debug messages using the Bareos debug message
 * facility.
 */
static PyObject* PyBareosDebugMessage(PyObject* self, PyObject* args)
{
  int level;
  char* dbgmsg = NULL;
  bpContext* bareos_plugin_ctx = NULL;

  if (!PyArg_ParseTuple(args, "i|z:BareosDebugMessage", &level, &dbgmsg)) {
    return NULL;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (dbgmsg) {
    bareos_plugin_ctx = GetPluginContextFromPythonModule();
    Dmsg(bareos_plugin_ctx, level, "python-sd: %s", dbgmsg);
  }

  Py_INCREF(Py_None);
  return Py_None;
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to issue Job messages using the Bareos Job message
 * facility.
 */
static PyObject* PyBareosJobMessage(PyObject* self, PyObject* args)
{
  int type;
  char* jobmsg = NULL;
  bpContext* bareos_plugin_ctx = NULL;

  if (!PyArg_ParseTuple(args, "i|z:BareosJobMessage", &type, &jobmsg)) {
    return NULL;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (jobmsg) {
    bareos_plugin_ctx = GetPluginContextFromPythonModule();
    Jmsg(bareos_plugin_ctx, type, "python-sd: %s", jobmsg);
  }

  Py_INCREF(Py_None);
  return Py_None;
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to issue a Register Event to register additional events
 * it wants to receive.
 */
static PyObject* PyBareosRegisterEvents(PyObject* self, PyObject* args)
{
  int len, event;
  bpContext* bareos_plugin_ctx = NULL;
  bRC retval = bRC_Error;
  PyObject *pyEvents, *pySeq, *pyEvent;

  if (!PyArg_ParseTuple(args, "O:BareosRegisterEvents", &pyEvents)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
  if (!pySeq) { goto bail_out; }

  len = PySequence_Fast_GET_SIZE(pySeq);

  bareos_plugin_ctx = GetPluginContextFromPythonModule();
  for (int i = 0; i < len; i++) {
    pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
    event = PyInt_AsLong(pyEvent);

    if (event >= bsdEventJobStart && event <= bsdEventWriteRecordTranslation) {
      Dmsg(bareos_plugin_ctx, debuglevel,
           "python-sd: PyBareosRegisterEvents registering event %d\n", event);
      retval = bfuncs->registerBareosEvents(bareos_plugin_ctx, 1, event);

      if (retval != bRC_OK) { break; }
    }
  }

  Py_DECREF(pySeq);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to issue an Unregister Event to unregister events it
 * doesn't want to receive anymore.
 */
static PyObject* PyBareosUnRegisterEvents(PyObject* self, PyObject* args)
{
  int len, event;
  bpContext* bareos_plugin_ctx = NULL;
  bRC retval = bRC_Error;
  PyObject *pyEvents, *pySeq, *pyEvent;

  if (!PyArg_ParseTuple(args, "O:BareosUnRegisterEvents", &pyEvents)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
  if (!pySeq) { goto bail_out; }

  len = PySequence_Fast_GET_SIZE(pySeq);

  bareos_plugin_ctx = GetPluginContextFromPythonModule();
  for (int i = 0; i < len; i++) {
    pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
    event = PyInt_AsLong(pyEvent);

    if (event >= bsdEventJobStart && event <= bsdEventWriteRecordTranslation) {
      Dmsg(bareos_plugin_ctx, debuglevel,
           "PyBareosUnRegisterEvents: registering event %d\n", event);
      retval = bfuncs->unregisterBareosEvents(bareos_plugin_ctx, 1, event);

      if (retval != bRC_OK) { break; }
    }
  }

  Py_DECREF(pySeq);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to issue a GetInstanceCount to retrieve the number of
 * instances of the current plugin being loaded into the daemon.
 */
static PyObject* PyBareosGetInstanceCount(PyObject* self, PyObject* args)
{
  int value;
  bpContext* bareos_plugin_ctx = NULL;
  PyObject* pRetVal = NULL;

  if (!PyArg_ParseTuple(args, ":BareosGetInstanceCount")) { return NULL; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  bareos_plugin_ctx = GetPluginContextFromPythonModule();
  if (!bareos_plugin_ctx) {
    PyErr_SetString(PyExc_ValueError, "bareos_plugin_ctx is unset");
    return NULL;
  }
  if (!bfuncs) {
    PyErr_SetString(PyExc_ValueError, "bfuncs is unset");
    return NULL;
  }
  if (bfuncs->getInstanceCount(bareos_plugin_ctx, &value) == bRC_OK) {
    pRetVal = PyInt_FromLong(value);
  }

  if (!pRetVal) {
    Py_INCREF(Py_None);
    pRetVal = Py_None;
  }

  return pRetVal;
}
} /* namespace storagedaemon*/
