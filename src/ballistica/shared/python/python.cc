// Released under the MIT License. See LICENSE for details.

#include "ballistica/shared/python/python.h"

#include "ballistica/core/support/base_soft.h"
#include "ballistica/shared/python/python_command.h"
#include "ballistica/shared/python/python_sys.h"

// Sanity test: our XCode, Android, and Windows builds should be
// using a debug build of the Python library.
// Todo: could also verify this at runtime by checking for
//  existence of sys.gettotalrefcount(). (is that still valid in 3.8?)
#if BA_XCODE_BUILD || BA_OSTYPE_ANDROID || BA_OSTYPE_WINDOWS
#if BA_DEBUG_BUILD
#ifndef Py_DEBUG
#error Expected Py_DEBUG to be defined for this build.
#endif  // Py_DEBUG
#else   // BA_DEBUG_BUILD
#ifdef Py_DEBUG
#error Expected Py_DEBUG to NOT be defined for this build.
#endif  // Py_DEBUG
#endif  // BA_DEBUG_BUILD
#endif  // BA_XCODE_BUILD || BA_OSTYPE_ANDROID

namespace ballistica {

// We implicitly use core functionality here; our behavior is undefined
// if nobody has imported core yet.
using core::g_base_soft;

// Ignore signed bitwise stuff; python macros do it quite a bit.
#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-signed-bitwise"
#pragma ide diagnostic ignored "RedundantCast"

void Python::SetPythonException(const Exception& exc) {
  PyExcType exctype{exc.python_type()};
  const char* description{GetShortExceptionDescription(exc)};
  PyObject* pytype{};
  switch (exctype) {
    case PyExcType::kRuntime:
      pytype = PyExc_RuntimeError;
      break;
    case PyExcType::kAttribute:
      pytype = PyExc_AttributeError;
      break;
    case PyExcType::kIndex:
      pytype = PyExc_IndexError;
      break;
    case PyExcType::kValue:
      pytype = PyExc_ValueError;
      break;
    case PyExcType::kType:
      pytype = PyExc_TypeError;
      break;
    default:
      // That's it for builtin exception types; all other values map
      // to custom error types defined in base.
      if (g_base_soft) {
        pytype = g_base_soft->GetPyExceptionType(exctype);
      }
      break;
  }
  // If base wasn't available or whatnot, tweak our error to make that known.
  // We should not be setting those extended types here in core so
  // this should never happen.
  if (pytype == nullptr) {
    description =
        "SetPythonException error type unavailable; should not happen.";
    pytype = PyExc_RuntimeError;
  }
  assert(pytype != nullptr && PyType_Check(pytype));
  PyErr_SetString(pytype, description);
}

const char* Python::ScopedCallLabel::current_label_ = nullptr;

auto Python::HaveGIL() -> bool { return static_cast<bool>(PyGILState_Check()); }

void Python::PrintStackTrace() {
  bool available{};
  if (g_base_soft) {
    available = g_base_soft->PrintPythonStackTrace();
  }
  if (!available) {
    Log(LogLevel::kWarning,
        "Python::PrintStackTrace() called before _babase set up; "
        "not printing.");
  }
}

auto Python::IsPyString(PyObject* o) -> bool {
  assert(HaveGIL());
  assert(o != nullptr);

  return PyUnicode_Check(o);
}

auto Python::GetPyString(PyObject* o) -> std::string {
  assert(HaveGIL());
  assert(o != nullptr);

  PyExcType exctype{PyExcType::kType};
  if (PyUnicode_Check(o)) {
    return PyUnicode_AsUTF8(o);
  }
  throw Exception(
      "Can't get string from value: " + Python::ObjToString(o) + ".", exctype);
}

template <typename T>
auto GetPyIntT(PyObject* o) -> T {
  assert(Python::HaveGIL());
  assert(o != nullptr);

  if (PyLong_Check(o)) {
    return static_cast_check_fit<T>(PyLong_AS_LONG(o));
  }
  if (PyNumber_Check(o)) {
    PyObject* f = PyNumber_Long(o);
    if (f) {
      auto val = static_cast_check_fit<T>(PyLong_AS_LONG(f));
      Py_DECREF(f);
      return val;
    }
  }

  // Failed, we have.
  // Clear any Python error that got us here; we're in C++ Exception land now.
  PyErr_Clear();

  // Assuming any failure here was type related.
  throw Exception("Can't get int from value: " + Python::ObjToString(o) + ".",
                  PyExcType::kType);
}

auto Python::GetPyInt64(PyObject* o) -> int64_t {
  return GetPyIntT<int64_t>(o);
}

auto Python::GetPyInt(PyObject* o) -> int { return GetPyIntT<int>(o); }

auto Python::GetPyBool(PyObject* o) -> bool {
  assert(HaveGIL());
  assert(o != nullptr);

  if (o == Py_True) {
    return true;
  }
  if (o == Py_False) {
    return false;
  }
  if (PyLong_Check(o)) {
    return (PyLong_AS_LONG(o) != 0);
  }
  if (PyNumber_Check(o)) {
    if (PyObject* o2 = PyNumber_Long(o)) {
      auto val = PyLong_AS_LONG(o2);
      Py_DECREF(o2);
      return (val != 0);
    }
  }

  // Failed, we have.
  // Clear any Python error that got us here; we're in C++ Exception land now.
  PyErr_Clear();

  // Assuming any failure here was type related.
  throw Exception("Can't get bool from value: " + Python::ObjToString(o) + ".",
                  PyExcType::kType);
}

auto Python::CanGetPyDouble(PyObject* o) -> bool {
  assert(HaveGIL());
  assert(o != nullptr);

  return static_cast<bool>(PyNumber_Check(o));
}

auto Python::GetPyDouble(PyObject* o) -> double {
  assert(HaveGIL());
  BA_PRECONDITION_FATAL(o != nullptr);

  // Try to take the fast path if its a float.
  if (PyFloat_Check(o)) {
    return PyFloat_AS_DOUBLE(o);
  }
  if (PyNumber_Check(o)) {
    if (PyObject* f = PyNumber_Float(o)) {
      double val = PyFloat_AS_DOUBLE(f);
      Py_DECREF(f);
      return val;
    }
  }

  // Failed, we have.
  // Clear any Python error that got us here; we're in C++ Exception land now.
  PyErr_Clear();
  throw Exception(
      "Can't get double from value: " + Python::ObjToString(o) + ".",
      PyExcType::kType);
}

auto Python::GetPyFloats(PyObject* o) -> std::vector<float> {
  assert(HaveGIL());
  BA_PRECONDITION_FATAL(o != nullptr);

  if (!PySequence_Check(o)) {
    throw Exception("Object is not a sequence.", PyExcType::kType);
  }
  PythonRef sequence(PySequence_Fast(o, "Not a sequence."), PythonRef::kSteal);
  assert(sequence.Exists());
  Py_ssize_t size = PySequence_Fast_GET_SIZE(sequence.Get());
  PyObject** py_objects = PySequence_Fast_ITEMS(sequence.Get());
  std::vector<float> vals(static_cast<size_t>(size));
  assert(vals.size() == size);
  for (Py_ssize_t i = 0; i < size; i++) {
    vals[i] = Python::GetPyFloat(py_objects[i]);
  }
  return vals;
}

template <typename T>
auto GetPyIntsT(PyObject* o) -> std::vector<T> {
  assert(Python::HaveGIL());
  BA_PRECONDITION_FATAL(o != nullptr);

  if (!PySequence_Check(o)) {
    throw Exception("Object is not a sequence.", PyExcType::kType);
  }
  PythonRef sequence(PySequence_Fast(o, "Not a sequence."), PythonRef::kSteal);
  assert(sequence.Exists());
  Py_ssize_t size = PySequence_Fast_GET_SIZE(sequence.Get());
  PyObject** pyobjs = PySequence_Fast_ITEMS(sequence.Get());
  std::vector<T> vals(static_cast<size_t>(size));
  assert(vals.size() == size);
  for (Py_ssize_t i = 0; i < size; i++) {
    vals[i] = GetPyIntT<T>(pyobjs[i]);
  }
  return vals;
}

auto Python::GetPyInts64(PyObject* o) -> std::vector<int64_t> {
  return GetPyIntsT<int64_t>(o);
}

auto Python::GetPyInts(PyObject* o) -> std::vector<int> {
  return GetPyIntsT<int>(o);
}

// Hmm should just template the above func?
auto Python::GetPyUInts64(PyObject* o) -> std::vector<uint64_t> {
  return GetPyIntsT<uint64_t>(o);
}

auto Python::GetPyPoint2D(PyObject* o) -> Point2D {
  assert(HaveGIL());
  BA_PRECONDITION_FATAL(o != nullptr);

  Point2D p;
  if (!PyTuple_Check(o) || (PyTuple_GET_SIZE(o) != 2)) {
    throw Exception("Expected 2 member tuple for point.", PyExcType::kType);
  }
  p.x = Python::GetPyFloat(PyTuple_GET_ITEM(o, 0));
  p.y = Python::GetPyFloat(PyTuple_GET_ITEM(o, 1));
  return p;
}

auto Python::StringList(const std::list<std::string>& values) -> PythonRef {
  assert(HaveGIL());
  auto size{static_cast<Py_ssize_t>(values.size())};
  PythonRef pylist{PyList_New(size), PythonRef::kSteal};
  int i{};
  for (auto&& value : values) {
    PyObject* item{PyUnicode_FromString(value.c_str())};
    assert(item);
    PyList_SET_ITEM(pylist.Get(), i, item);
    ++i;
  }
  return pylist;
}

auto Python::SingleMemberTuple(const PythonRef& member) -> PythonRef {
  assert(HaveGIL());
  return {Py_BuildValue("(O)", member.NewRef()), PythonRef::kSteal};
}

auto Python::GetPythonFileLocation(bool pretty) -> std::string {
  assert(HaveGIL());
  if (PyFrameObject* f = PyEval_GetFrame()) {
    const char* path;
    auto code_obj =
        PythonRef::Stolen(reinterpret_cast<PyObject*>(PyFrame_GetCode(f)));
    auto* code = reinterpret_cast<PyCodeObject*>(code_obj.Get());
    assert(code);
    if (code && code->co_filename) {
      assert(PyUnicode_Check(code->co_filename));
      path = PyUnicode_AsUTF8(code->co_filename);
      if (pretty) {
        if (path[0] == '<') {
          // Don't attempt to parse stuff like <string:/foo/bar.cpp line 724>:1
          return "<internal>";
        } else {
          // Advance past any '/' and '\'s
          while (true) {
            const char* s = strchr(path, '/');
            if (s) {
              path = s + 1;
            } else {
              const char* s2 = strchr(path, '\\');
              if (s2) {
                path = s2 + 1;
              } else {
                break;
              }
            }
          }
        }
      }
    } else {
      path = "<filename_unavailable>";
    }
    std::string name =
        std::string(path) + ":" + std::to_string(PyFrame_GetLineNumber(f));
    return name;
  }
  return "<unknown>";
}

auto Python::GetContextBaseString() -> std::string {
  // Allow this to survive before core is bootstrapped.
  if (!g_base_soft) {
    return "  context_ref: <base not yet bootstrapped>";
  }
  return g_base_soft->DoGetContextBaseString();
}

void Python::PrintContextNotYetBootstrapped() {
  // (no logic-thread-check here; can be called early or from other threads)
  std::string s = std::string("  root call: <not yet bootstrapped>\n");
  s += Python::GetContextBaseString();
  PySys_WriteStderr("%s\n", s.c_str());
}

void Python::PrintContextAuto() {
  // Lets print whatever context info is available.
  // FIXME: If we have recursive calls this may not print
  //  the context we'd expect; we'd need a unified stack.
  if (!g_base_soft) {
    PrintContextNotYetBootstrapped();
  }
}

auto Python::ObjToString(PyObject* obj) -> std::string {
  if (obj) {
    return PythonRef(obj, PythonRef::kAcquire).Str();
  } else {
    return "<nullptr PyObject*>";
  }
}

auto Python::ObjTypeToString(PyObject* obj) -> std::string {
  if (obj) {
    return PythonRef(obj, PythonRef::kAcquire).Type().Str();
  } else {
    return "<nullptr PyObject*>";
  }
}

void Python::MarkReachedEndOfModule(PyObject* module) {
  auto* val = Py_True;
  Py_INCREF(val);
  auto result = PyObject_SetAttrString(module, "_REACHED_END_OF_MODULE", val);
  assert(result == 0);
}

class Python::ScopedInterpreterLock::Impl {
 public:
  Impl() {
    if (need_lock_) {
      // Grab the python GIL.
      gstate_ = PyGILState_Ensure();
    }
  }
  ~Impl() {
    if (need_lock_) {
      // Release the python GIL.
      PyGILState_Release(gstate_);
    }
  }

 private:
  bool need_lock_{true};
  PyGILState_STATE gstate_{PyGILState_UNLOCKED};
};

Python::ScopedInterpreterLock::ScopedInterpreterLock()
    : impl_{new Python::ScopedInterpreterLock::Impl()}
// impl_{std::make_unique<Python::ScopedInterpreterLock::Impl>()}
{}

Python::ScopedInterpreterLock::~ScopedInterpreterLock() { delete impl_; }

// (some stuff borrowed from python's source code - used in our overriding of
// objects' dir() results)

/* alphabetical order */
_Py_IDENTIFIER(__class__);
_Py_IDENTIFIER(__dict__);

/* ------------------------- PyObject_Dir() helpers ------------------------- */

/*
 Merge the __dict__ of aclass into dict, and recursively also all
 the __dict__s of aclass's base classes.  The order of merging isn't
 defined, as it's expected that only the final set of dict keys is
 interesting.
 Return 0 on success, -1 on error.
 */

static auto merge_class_dict(PyObject* dict, PyObject* aclass) -> int {
  PyObject* classdict;
  PyObject* bases;
  _Py_IDENTIFIER(__bases__);

  assert(PyDict_Check(dict));
  assert(aclass);

  /* Merge in the type's dict (if any). */
  classdict = _PyObject_GetAttrId(aclass, &PyId___dict__);
  if (classdict == nullptr) {
    PyErr_Clear();
  } else {
    int status = PyDict_Update(dict, classdict);
    Py_DECREF(classdict);
    if (status < 0) return -1;
  }

  /* Recursively merge in the base types' (if any) dicts. */
  bases = _PyObject_GetAttrId(aclass, &PyId___bases__);
  if (bases == nullptr) {
    PyErr_Clear();
  } else {
    /* We have no guarantee that bases is a real tuple */
    Py_ssize_t i;
    Py_ssize_t n;
    n = PySequence_Size(bases); /* This better be right */
    if (n < 0) {
      PyErr_Clear();
    } else {
      for (i = 0; i < n; i++) {
        int status;
        PyObject* base = PySequence_GetItem(bases, i);
        if (base == nullptr) {
          Py_DECREF(bases);
          return -1;
        }
        status = merge_class_dict(dict, base);
        Py_DECREF(base);
        if (status < 0) {
          Py_DECREF(bases);
          return -1;
        }
      }
    }
    Py_DECREF(bases);
  }
  return 0;
}

/* __dir__ for generic objects: returns __dict__, __class__,
 and recursively up the __class__.__bases__ chain.
 */
auto Python::generic_dir(PyObject* self) -> PyObject* {
  PyObject* result = nullptr;
  PyObject* dict = nullptr;
  PyObject* itsclass = nullptr;

  /* Get __dict__ (which may or may not be a real dict...) */
  dict = _PyObject_GetAttrId(self, &PyId___dict__);
  if (dict == nullptr) {
    PyErr_Clear();
    dict = PyDict_New();
  } else if (!PyDict_Check(dict)) {
    Py_DECREF(dict);
    dict = PyDict_New();
  } else {
    /* Copy __dict__ to avoid mutating it. */
    PyObject* temp = PyDict_Copy(dict);
    Py_DECREF(dict);
    dict = temp;
  }

  if (dict == nullptr) goto error;

  /* Merge in attrs reachable from its class. */
  itsclass = _PyObject_GetAttrId(self, &PyId___class__);
  if (itsclass == nullptr)
    /* XXX(tomer): Perhaps fall back to obj->ob_type if no
     __class__ exists? */
    PyErr_Clear();
  else if (merge_class_dict(dict, itsclass) != 0)
    goto error;

  result = PyDict_Keys(dict);
  /* fall through */
error:
  Py_XDECREF(itsclass);
  Py_XDECREF(dict);
  return result;
}
////////////////   end __dir__ helpers

#pragma clang diagnostic pop

}  // namespace ballistica
