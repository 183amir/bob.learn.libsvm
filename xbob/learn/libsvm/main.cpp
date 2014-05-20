/**
 * @author Andre Anjos <andre.anjos@idiap.ch>
 * @date Fri 13 Dec 2013 12:35:59 CET
 *
 * @brief Bindings to bob::machine
 */

#define XBOB_LEARN_LIBSVM_MODULE
#include <xbob.learn.libsvm/api.h>

#ifdef NO_IMPORT_ARRAY
#undef NO_IMPORT_ARRAY
#endif
#include <xbob.blitz/capi.h>
#include <xbob.blitz/cleanup.h>
#include <xbob.io.base/api.h>

static PyMethodDef module_methods[] = {
    {0}  /* Sentinel */
};

PyDoc_STRVAR(module_docstr, "bob::learn::libsvm's machine and trainers");

int PyXbobLearnLibsvm_APIVersion = XBOB_LEARN_LIBSVM_API_VERSION;

#if PY_VERSION_HEX >= 0x03000000
static PyModuleDef module_definition = {
  PyModuleDef_HEAD_INIT,
  XBOB_EXT_MODULE_NAME,
  module_docstr,
  -1,
  module_methods,
  0, 0, 0, 0
};
#endif

static PyObject* create_module (void) {

  PyBobLearnLibsvmFile_Type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&PyBobLearnLibsvmFile_Type) < 0) return 0;

  PyBobLearnLibsvmMachine_Type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&PyBobLearnLibsvmMachine_Type) < 0) return 0;

  PyBobLearnLibsvmTrainer_Type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&PyBobLearnLibsvmTrainer_Type) < 0) return 0;

# if PY_VERSION_HEX >= 0x03000000
  PyObject* m = PyModule_Create(&module_definition);
# else
  PyObject* m = Py_InitModule3(XBOB_EXT_MODULE_NAME, module_methods, module_docstr);
# endif
  if (!m) return 0;
  auto m_ = make_safe(m);

  /* register some constants */
  if (PyModule_AddIntConstant(m, "__api_version__", XBOB_LEARN_LIBSVM_API_VERSION) < 0) return 0;
  if (PyModule_AddStringConstant(m, "__version__", XBOB_EXT_MODULE_VERSION) < 0) return 0;

  /* register the types to python */
  Py_INCREF(&PyBobLearnLibsvmFile_Type);
  if (PyModule_AddObject(m, "File", (PyObject *)&PyBobLearnLibsvmFile_Type) < 0) return 0;

  Py_INCREF(&PyBobLearnLibsvmMachine_Type);
  if (PyModule_AddObject(m, "Machine", (PyObject *)&PyBobLearnLibsvmMachine_Type) < 0) return 0;

  Py_INCREF(&PyBobLearnLibsvmTrainer_Type);
  if (PyModule_AddObject(m, "Trainer", (PyObject *)&PyBobLearnLibsvmTrainer_Type) < 0) return 0;

  static void* PyXbobLearnLibsvm_API[PyXbobLearnLibsvm_API_pointers];

  /* exhaustive list of C APIs */

  /**************
   * Versioning *
   **************/

  PyXbobLearnLibsvm_API[PyXbobLearnLibsvm_APIVersion_NUM] = (void *)&PyXbobLearnLibsvm_APIVersion;

  /***************************************
   * Bindings for xbob.learn.libsvm.File *
   ***************************************/

  PyXbobLearnLibsvm_API[PyBobLearnLibsvmFile_Type_NUM] = (void *)&PyBobLearnLibsvmFile_Type;

  PyXbobLearnLibsvm_API[PyBobLearnLibsvmFile_Check_NUM] = (void *)&PyBobLearnLibsvmFile_Check;

  /******************************************
   * Bindings for xbob.learn.libsvm.Machine *
   ******************************************/

  PyXbobLearnLibsvm_API[PyBobLearnLibsvmMachine_Type_NUM] = (void *)&PyBobLearnLibsvmMachine_Type;

  PyXbobLearnLibsvm_API[PyBobLearnLibsvmMachine_Check_NUM] = (void *)&PyBobLearnLibsvmMachine_Check;

  PyXbobLearnLibsvm_API[PyBobLearnLibsvmMachine_NewFromMachine_NUM] = (void *)&PyBobLearnLibsvmMachine_NewFromMachine;

  /******************************************
   * Bindings for xbob.learn.libsvm.Trainer *
   ******************************************/

  PyXbobLearnLibsvm_API[PyBobLearnLibsvmTrainer_Type_NUM] = (void *)&PyBobLearnLibsvmTrainer_Type;

  PyXbobLearnLibsvm_API[PyBobLearnLibsvmTrainer_Check_NUM] = (void *)&PyBobLearnLibsvmTrainer_Check;

  /*********************************
   * Bindings to general utilities *
   *********************************/

  PyXbobLearnLibsvm_API[PyBobLearnLibsvm_MachineTypeAsString_NUM] = (void *)&PyBobLearnLibsvm_MachineTypeAsString;

  PyXbobLearnLibsvm_API[PyBobLearnLibsvm_StringAsMachineType_NUM] = (void *)&PyBobLearnLibsvm_StringAsMachineType;

  PyXbobLearnLibsvm_API[PyBobLearnLibsvm_CStringAsMachineType_NUM] = (void *)&PyBobLearnLibsvm_CStringAsMachineType;

  PyXbobLearnLibsvm_API[PyBobLearnLibsvm_KernelTypeAsString_NUM] = (void *)&PyBobLearnLibsvm_KernelTypeAsString;

  PyXbobLearnLibsvm_API[PyBobLearnLibsvm_StringAsKernelType_NUM] = (void *)&PyBobLearnLibsvm_StringAsKernelType;

  PyXbobLearnLibsvm_API[PyBobLearnLibsvm_CStringAsKernelType_NUM] = (void *)&PyBobLearnLibsvm_CStringAsKernelType;

#if PY_VERSION_HEX >= 0x02070000

  /* defines the PyCapsule */

  PyObject* c_api_object = PyCapsule_New((void *)PyXbobLearnLibsvm_API,
      XBOB_EXT_MODULE_PREFIX "." XBOB_EXT_MODULE_NAME "._C_API", 0);

#else

  PyObject* c_api_object = PyCObject_FromVoidPtr((void *)PyXbobLearnLibsvm_API, 0);

#endif

  if (c_api_object) PyModule_AddObject(m, "_C_API", c_api_object);

  /* imports dependencies */
  if (import_xbob_blitz() < 0) {
    PyErr_Print();
    PyErr_Format(PyExc_ImportError, "cannot import `%s'", XBOB_EXT_MODULE_NAME);
    return 0;
  }

  if (import_xbob_io_base() < 0) {
    PyErr_Print();
    PyErr_Format(PyExc_ImportError, "cannot import `%s'", XBOB_EXT_MODULE_NAME);
    return 0;
  }

  Py_INCREF(m);
  return m;

}

PyMODINIT_FUNC XBOB_EXT_ENTRY_NAME (void) {
# if PY_VERSION_HEX >= 0x03000000
  return
# endif
    create_module();
}
