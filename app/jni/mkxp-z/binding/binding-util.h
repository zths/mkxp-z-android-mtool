/*
 ** binding-util.h
 **
 ** This file is part of mkxp.
 **
 ** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
 **
 ** mkxp is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** mkxp is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BINDING_UTIL_H
#define BINDING_UTIL_H

#include <ruby.h>
#ifndef MKXPZ_LEGACY_RUBY
#include <ruby/version.h>
#else
#include <version.h>
#endif

#include "exception.h"

#ifdef RUBY_API_VERSION_MAJOR
#define RAPI_MAJOR RUBY_API_VERSION_MAJOR
#define RAPI_MINOR RUBY_API_VERSION_MINOR
#define RAPI_TEENY RUBY_API_VERSION_TEENY
#else
#define RAPI_MAJOR RUBY_VERSION_MAJOR
#define RAPI_MINOR RUBY_VERSION_MINOR
#define RAPI_TEENY RUBY_VERSION_TEENY
#endif
#define RAPI_FULL ((RAPI_MAJOR * 100) + (RAPI_MINOR * 10) + RAPI_TEENY)

// Ruby版本兼容性函数
#if RAPI_FULL <= 187
// Ruby 1.8.7版本的兼容性定义
static inline VALUE compat_rb_errinfo(void) {
  return ruby_errinfo;
}

static inline void compat_rb_set_errinfo(VALUE err) {
  ruby_errinfo = err;
}

static inline VALUE compat_rb_str_new_cstr(const char* str) {
    return rb_str_new2(str);
}

#define rb_errinfo() compat_rb_errinfo()
#define rb_set_errinfo(err) compat_rb_set_errinfo(err)
#define rb_str_new_cstr(str) compat_rb_str_new_cstr(str)

#elif RAPI_FULL >= 190 && RAPI_FULL < 220
// Ruby 1.9.x版本的兼容性定义
// rb_str_new_cstr在1.9.x中可用，但为了保险起见，检查是否存在
#ifndef rb_str_new_cstr
#define rb_str_new_cstr(str) rb_str_new2(str)
#endif

#endif

enum RbException {
    RGSS = 0,
    Reset,
    PHYSFS,
    SDL,
    MKXP,

    ErrnoENOENT,

    IOError,

    TypeError,
    ArgumentError,
    SystemExit,
    RuntimeError,

    RbExceptionsMax
};

struct RbData {
    VALUE exc[RbExceptionsMax];

    /* Input module (RGSS3) */
    VALUE buttoncodeHash;

    RbData();
    ~RbData();
};

RbData *getRbData();

struct Exception;

void raiseRbExc(Exception *exc);

#if RAPI_MAJOR >= 2
void *drop_gvl_guard(void *(*func)(void *), void *args,
                            rb_unblock_function_t *ubf, void *data2);
#endif

#if RAPI_FULL > 187
#define DECL_TYPE(Klass) extern rb_data_type_t Klass##Type

/* 2.1 has added a new field (flags) to rb_data_type_t */
#if RAPI_FULL >= 210
/* TODO: can mkxp use RUBY_TYPED_FREE_IMMEDIATELY here? */
#define DEF_TYPE_FLAGS 0
#else
#define DEF_TYPE_FLAGS
#endif
#endif

#if RAPI_MAJOR > 1 || RAPI_MINOR <= 9
#if RAPI_FULL < 270
#define DEF_TYPE_CUSTOMNAME_AND_FREE(Klass, Name, Free)                        \
rb_data_type_t Klass##Type = {                                               \
Name, {0, Free, 0, {0, 0}}, 0, 0, DEF_TYPE_FLAGS}
#else
#define DEF_TYPE_CUSTOMNAME_AND_FREE(Klass, Name, Free)                        \
rb_data_type_t Klass##Type = {Name, {0, Free, 0, 0, 0}, 0, 0, DEF_TYPE_FLAGS}
#endif

#define DEF_TYPE_CUSTOMFREE(Klass, Free)                                       \
DEF_TYPE_CUSTOMNAME_AND_FREE(Klass, #Klass, Free)

#define DEF_TYPE_CUSTOMNAME(Klass, Name)                                       \
DEF_TYPE_CUSTOMNAME_AND_FREE(Klass, Name, freeInstance<Klass>)

#define DEF_TYPE(Klass) DEF_TYPE_CUSTOMNAME(Klass, #Klass)
#endif

// Ruby 1.8 helper stuff
#if RAPI_MAJOR < 2

#if RAPI_MINOR < 9
#define RUBY_T_FIXNUM T_FIXNUM
#define RUBY_T_TRUE T_TRUE
#define RUBY_T_FALSE T_FALSE
#define RUBY_T_NIL T_NIL
#define RUBY_T_UNDEF T_UNDEF
#define RUBY_T_SYMBOL T_SYMBOL
#define RUBY_T_FLOAT T_FLOAT
#define RUBY_T_STRING T_STRING
#define RUBY_T_ARRAY T_ARRAY
#else
#define T_FIXNUM RUBY_T_FIXNUM
#define T_TRUE RUBY_T_TRUE
#define T_FALSE RUBY_T_FALSE
#define T_NIL RUBY_T_NIL
#define T_UNDEF RUBY_T_UNDEF
#define T_SYMBOL RUBY_T_SYMBOL
#define T_FLOAT RUBY_T_FLOAT
#define T_STRING RUBY_T_STRING
#define T_ARRAY RUBY_T_ARRAY
#endif

#if RAPI_MINOR < 9
#define RUBY_Qtrue Qtrue
#define RUBY_Qfalse Qfalse
#define RUBY_Qnil Qnil
#define RUBY_Qundef Qundef
#endif

#if RAPI_MINOR < 9
#define RB_FIXNUM_P(obj) FIXNUM_P(obj)
#define RB_SYMBOL_P(obj) SYMBOL_P(obj)

#define RB_TYPE_P(obj, type)                                                   \
(((type) == RUBY_T_FIXNUM)                                                   \
? RB_FIXNUM_P(obj)                                                      \
: ((type) == RUBY_T_TRUE)                                               \
? ((obj) == RUBY_Qtrue)                                           \
: ((type) == RUBY_T_FALSE)                                        \
? ((obj) == RUBY_Qfalse)                                    \
: ((type) == RUBY_T_NIL)                                    \
? ((obj) == RUBY_Qnil)                                \
: ((type) == RUBY_T_UNDEF)                            \
? ((obj) == RUBY_Qundef)                        \
: ((type) == RUBY_T_SYMBOL)                     \
? RB_SYMBOL_P(obj)                        \
: (!SPECIAL_CONST_P(obj) &&               \
BUILTIN_TYPE(obj) == (type)))
#endif

#define OBJ_INIT_COPY(a, b) rb_obj_init_copy(a, b)

#define DEF_ALLOCFUNC_CUSTOMFREE(type, free)                                   \
static VALUE type##Allocate(VALUE klass) {                                   \
return Data_Wrap_Struct(klass, 0, free, 0);                                \
}

#define DEF_ALLOCFUNC(type) DEF_ALLOCFUNC_CUSTOMFREE(type, freeInstance<type>)

#define PRIsVALUE "s"

#endif

#if RAPI_FULL < 220
#define rb_utf8_str_new_cstr rb_str_new2
#define rb_utf8_str_new rb_str_new
#endif

// end

#if RAPI_FULL > 187
template <rb_data_type_t *rbType> static VALUE classAllocate(VALUE klass) {
    /* 2.3 has changed the name of this function */
#if RAPI_FULL >= 230
    return rb_data_typed_object_wrap(klass, 0, rbType);
#else
    return rb_data_typed_object_alloc(klass, 0, rbType);
#endif
}
#endif

#if RAPI_FULL > 187
#define CLASS_ALLOCATE_PRE_INIT(Name, initializeFunc)  \
static VALUE Name##AllocatePreInit(VALUE klass) {      \
  VALUE ret = classAllocate<& Name##Type>(klass);     \
                                                       \
  initializeFunc(0, 0, ret);                           \
                                                       \
  return ret;                                          \
}
#else
#define CLASS_ALLOCATE_PRE_INIT(Name, initializeFunc)  \
static VALUE Name##AllocatePreInit(VALUE klass) {      \
  VALUE ret = Name##Allocate(klass);                   \
                                                       \
  initializeFunc(0, 0, ret);                           \
                                                       \
  return ret;                                          \
}
#endif

template <class C> static void freeInstance(void *inst) {
    delete static_cast<C *>(inst);
}

void raiseDisposedAccess(VALUE self);

template <class C> inline C *getPrivateDataNoRaise(VALUE self) {
#if RAPI_FULL > 187
    return static_cast<C *>(RTYPEDDATA_DATA(self));
#else
    return static_cast<C *>(DATA_PTR(self));
#endif
}

template <class C> inline C *getPrivateData(VALUE self) {
    C *c = getPrivateDataNoRaise<C>(self);
    
    if (!c) {
        //raiseRbExc(Exception(Exception::MKXPError, "No instance data for variable (missing call to super?)"));
        
        /* FIXME: MiniFFI and FileInt don't have default allocations
         * despite not being disposables. Should they be fixed,
         * or just left with a misleading error message? */
        raiseDisposedAccess(self);
    }
    return c;
}

template <class C>
static inline C *
#if RAPI_FULL > 187
getPrivateDataCheck(VALUE self, const rb_data_type_t &type)
#else
getPrivateDataCheck(VALUE self, const char *type)
#endif
{
#if RAPI_FULL <= 187
    rb_check_type(self, T_DATA);
    VALUE otherObj = rb_const_get(rb_cObject, rb_intern(type));
    const char *ownname, *othername;
    if (!rb_obj_is_kind_of(self, otherObj)) {
        ownname = rb_obj_classname(self);
        othername = rb_obj_classname(otherObj);
        rb_raise(rb_eTypeError, "Can't convert %s into %s", othername, ownname);
    }
    void *obj = DATA_PTR(self);
#else
    const char *ownname = rb_obj_classname(self);
    if (!rb_typeddata_is_kind_of(self, &type))
        rb_raise(rb_eTypeError, "Can't convert %s into %s", ownname,
                 type.wrap_struct_name);

    void *obj = RTYPEDDATA_DATA(self);
#endif
    return static_cast<C *>(obj);
}

static inline void setPrivateData(VALUE self, void *p) {
    /* RGSS's behavior is to just leak memory if a disposable is reinitialized,
     * with the original disposable being left permanently instantiated,
     * but that's (1) bad, and (2) would currently cause memory access issues
     * when things like a sprite's src_rect inevitably get GC'd, so we're not
     * copying that. */
#if RAPI_FULL > 187
    // Free the old value if it already exists (initialize called twice?)
    if (RTYPEDDATA_DATA(self) && (RTYPEDDATA_DATA(self) != p)) {
        /* RUBY_TYPED_NEVER_FREE == 0, and we don't use
         * RUBY_TYPED_DEFAULT_FREE for our stuff, so just
         * checking if it's truthy should be fine */
        if (RTYPEDDATA_TYPE(self)->function.dfree)
            (*RTYPEDDATA_TYPE(self)->function.dfree)(RTYPEDDATA_DATA(self));
    }
    RTYPEDDATA_DATA(self) = p;
#else
    // Free the old value if it already exists (initialize called twice?)
    if (DATA_PTR(self) && (DATA_PTR(self) != p)) {
        /* As above, just check if it's truthy */
        if (RDATA(self)->dfree)
            (*RDATA(self)->dfree)(DATA_PTR(self));
    }
    DATA_PTR(self) = p;
#endif
}

inline VALUE
#if RAPI_FULL > 187
wrapObject(void *p, const rb_data_type_t &type, VALUE underKlass = rb_cObject)
#else
wrapObject(void *p, const char *type, VALUE underKlass = rb_cObject)
#endif
{
#if RAPI_FULL > 187
    VALUE klass = rb_const_get(underKlass, rb_intern(type.wrap_struct_name));
#else
    VALUE klass = rb_const_get(underKlass, rb_intern(type));
#endif
    VALUE obj = rb_obj_alloc(klass);

    setPrivateData(obj, p);

    return obj;
}

inline VALUE wrapProperty(VALUE self, void *prop, const char *iv,
#if RAPI_FULL > 187
                          const rb_data_type_t &type,
#else
        const char *type,
#endif
                          VALUE underKlass = rb_cObject) {
    VALUE propObj = wrapObject(prop, type, underKlass);

    rb_iv_set(self, iv, propObj);

    return propObj;
}

/* Implemented: oSszfibn| */
int rb_get_args(int argc, VALUE *argv, const char *format, ...);

/* Always terminate 'rb_get_args' with this */
#ifndef NDEBUG
#define RB_ARG_END_VAL ((void *)-1)
#define RB_ARG_END , RB_ARG_END_VAL
#else
#define RB_ARG_END
#endif

typedef VALUE (*RubyMethod)(int argc, VALUE *argv, VALUE self);

static inline void _rb_define_method(VALUE klass, const char *name,
                                     RubyMethod func) {
    rb_define_method(klass, name, RUBY_METHOD_FUNC(func), -1);
}

static inline void rb_define_class_method(VALUE klass, const char *name,
                                          RubyMethod func) {
    rb_define_singleton_method(klass, name, RUBY_METHOD_FUNC(func), -1);
}

static inline void _rb_define_module_function(VALUE module, const char *name,
                                              RubyMethod func) {
    rb_define_module_function(module, name, RUBY_METHOD_FUNC(func), -1);
}

#define GFX_GUARD_EXC(exp)                                               \
{                                                                        \
GFX_LOCK;                                                                \
try {                                                                    \
exp                                                                      \
} catch (const Exception &exc) {                                         \
GFX_UNLOCK;                                                              \
throw exc;                                                               \
}                                                                        \
GFX_UNLOCK;                                                              \
}


template <class C>
static inline VALUE objectLoad(int argc, VALUE *argv, VALUE self) {
    const char *data;
    int dataLen;
    rb_get_args(argc, argv, "s", &data, &dataLen RB_ARG_END);

    VALUE obj = rb_obj_alloc(self);

    C *c = 0;

    c = C::deserialize(data, dataLen);

    setPrivateData(obj, c);

    return obj;
}

static inline VALUE rb_bool_new(bool value) { return value ? Qtrue : Qfalse; }

inline void rb_float_arg(VALUE arg, double *out, int argPos = 0) {
    switch (rb_type(arg)) {
        case RUBY_T_FLOAT:
            *out = RFLOAT_VALUE(arg);
            break;

        case RUBY_T_FIXNUM:
            *out = FIX2INT(arg);
            break;

        default:
            throw Exception(Exception::TypeError, "Argument %d: Expected float", argPos);
    }
}

inline void rb_int_arg(VALUE arg, int *out, int argPos = 0) {
    switch (rb_type(arg)) {
        case RUBY_T_FLOAT:
            // FIXME check int range?
            *out = NUM2LONG(arg);
            break;

        case RUBY_T_FIXNUM:
            *out = FIX2INT(arg);
            break;

        default:
            throw Exception(Exception::TypeError, "Argument %d: Expected fixnum", argPos);
    }
}

inline void rb_bool_arg(VALUE arg, bool *out, int argPos = 0) {
    switch (rb_type(arg)) {
        case RUBY_T_TRUE:
            *out = true;
            break;

        case RUBY_T_FALSE:
        case RUBY_T_NIL:
            *out = false;
            break;

        default:
            throw Exception(Exception::TypeError, "Argument %d: Expected bool", argPos);
    }
}

/* rb_check_argc and rb_error_arity are both
 * consistently called before any C++ objects are allocated,
 * so we can just call rb_raise directly in them */
inline void rb_check_argc(int actual, int expected) {
    if (actual != expected)
        rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", actual,
                 expected);
}

#if RAPI_MAJOR < 2
static inline void rb_error_arity(int argc, int min, int max) {
    if (argc > max || argc < min)
        rb_raise(rb_eArgError, "Finish me! rb_error_arity()"); // TODO
}

#if RAPI_MINOR < 9
static inline VALUE rb_sprintf(const char *fmt, ...) {
    return rb_str_new2("Finish me! rb_sprintf()"); // TODO
}

static inline VALUE rb_str_catf(VALUE obj, const char *fmt, ...) {
    return rb_str_new2("Finish me! rb_str_catf()"); // TODO
}

static inline VALUE rb_file_open_str(VALUE filename, const char *mode) {
    return rb_funcall(rb_cFile, rb_intern("open"), 2, filename,
                      rb_str_new2(mode));
}
#endif
#endif

#define RB_METHOD(name) static VALUE name(int argc, VALUE *argv, VALUE self)

#define RB_UNUSED_PARAM                                                        \
{                                                                            \
(void)argc;                                                                \
(void)argv;                                                                \
(void)self;                                                                \
}

/* Calling rb_raise inside the catch block
 * leaks memory even if we catch by value */
#define RB_METHOD_GUARD(name) RB_METHOD(name)   \
{                                               \
    Exception *exc = 0;                         \
    try{                                        \

#define RB_METHOD_GUARD_END                     \
    } catch (const Exception &e) {              \
        exc = new Exception(e);                 \
    }                                           \
    if (exc) {                                  \
        raiseRbExc(exc);                        \
    }                                           \
    return Qnil;                                \
}

#define MARSH_LOAD_FUN(Typ)                                                    \
RB_METHOD_GUARD(Typ##Load) { return objectLoad<Typ>(argc, argv, self); } RB_METHOD_GUARD_END

#define INITCOPY_FUN(Klass)                                                    \
RB_METHOD_GUARD(Klass##InitializeCopy) {                                   \
VALUE origObj;                                                             \
rb_get_args(argc, argv, "o", &origObj RB_ARG_END);                         \
if (!OBJ_INIT_COPY(self, origObj)) /* When would this fail??*/             \
return self;                                                             \
Klass *orig = getPrivateData<Klass>(origObj);                              \
Klass *k = 0;                                                              \
k = new Klass(*orig);                                                      \
setPrivateData(self, k);                                                   \
return self;                                                               \
}                                                                          \
RB_METHOD_GUARD_END

/* Object property which is copied by reference, with allowed NIL
 * FIXME: Getter assumes prop is disposable,
 * because self.disposed? is not checked in this case.
 * Should make this more clear */

// --------------
// Do not wait for Graphics.update
// --------------
#if RAPI_FULL > 187
#define DEF_PROP_OBJ_REF(Klass, PropKlass, PropName, prop_iv)                  \
RB_METHOD(Klass##Get##PropName) {                                            \
RB_UNUSED_PARAM;                                                           \
return rb_iv_get(self, prop_iv);                                           \
}                                                                            \
RB_METHOD_GUARD(Klass##Set##PropName) {                                    \
RB_UNUSED_PARAM;                                                           \
rb_check_argc(argc, 1);                                                    \
Klass *k = getPrivateData<Klass>(self);                                    \
VALUE propObj = *argv;                                                     \
PropKlass *prop;                                                           \
if (NIL_P(propObj))                                                        \
prop = 0;                                                                \
else                                                                       \
prop = getPrivateDataCheck<PropKlass>(propObj, PropKlass##Type);         \
k->set##PropName(prop)                                                     \
rb_iv_set(self, prop_iv, propObj);                                         \
return propObj;                                                            \
}                                                                          \
RB_METHOD_GUARD_END
#else
#define DEF_PROP_OBJ_REF(Klass, PropKlass, PropName, prop_iv)                  \
RB_METHOD(Klass##Get##PropName) {                                            \
RB_UNUSED_PARAM;                                                           \
return rb_iv_get(self, prop_iv);                                           \
}                                                                            \
RB_METHOD_GUARD(Klass##Set##PropName) {                                    \
RB_UNUSED_PARAM;                                                           \
rb_check_argc(argc, 1);                                                    \
Klass *k = getPrivateData<Klass>(self);                                    \
VALUE propObj = *argv;                                                     \
PropKlass *prop;                                                           \
if (NIL_P(propObj))                                                        \
prop = 0;                                                                \
else                                                                       \
prop = getPrivateDataCheck<PropKlass>(propObj, #PropKlass);              \
k->set##PropName(prop);                                                     \
rb_iv_set(self, prop_iv, propObj);                                         \
return propObj;                                                            \
}                                                                          \
RB_METHOD_GUARD_END
#endif

/* Object property which is copied by value, not reference */
#if RAPI_FULL > 187
#define DEF_PROP_OBJ_VAL(Klass, PropKlass, PropName, prop_iv)                  \
RB_METHOD(Klass##Get##PropName) {                                            \
RB_UNUSED_PARAM;                                                           \
checkDisposed<Klass>(self);                                                \
return rb_iv_get(self, prop_iv);                                           \
}                                                                            \
RB_METHOD_GUARD(Klass##Set##PropName) {                                    \
rb_check_argc(argc, 1);                                                    \
Klass *k = getPrivateData<Klass>(self);                                    \
VALUE propObj = *argv;                                                     \
PropKlass *prop;                                                           \
prop = getPrivateDataCheck<PropKlass>(propObj, PropKlass##Type);           \
k->set##PropName(*prop);                                                   \
return propObj;                                                            \
}                                                                          \
RB_METHOD_GUARD_END
#else
#define DEF_PROP_OBJ_VAL(Klass, PropKlass, PropName, prop_iv)                  \
RB_METHOD(Klass##Get##PropName) {                                            \
RB_UNUSED_PARAM;                                                           \
checkDisposed<Klass>(self);                                                \
return rb_iv_get(self, prop_iv);                                           \
}                                                                            \
RB_METHOD_GUARD(Klass##Set##PropName) {                                    \
rb_check_argc(argc, 1);                                                    \
Klass *k = getPrivateData<Klass>(self);                                    \
VALUE propObj = *argv;                                                     \
PropKlass *prop;                                                           \
            prop = getPrivateDataCheck<PropKlass>(propObj, #PropKlass);                \
            k->set##PropName(*prop);                                                   \
return propObj;                                                            \
}                                                                          \
RB_METHOD_GUARD_END
#endif

#define DEF_PROP(Klass, type, PropName, arg_fun, value_fun)                \
RB_METHOD_GUARD(Klass##Get##PropName) {                                    \
RB_UNUSED_PARAM;                                                           \
Klass *k = getPrivateData<Klass>(self);                                    \
type value = 0;                                                            \
value = k->get##PropName();                                                \
return value_fun(value);                                                   \
}                                                                          \
RB_METHOD_GUARD_END                                                        \
RB_METHOD_GUARD(Klass##Set##PropName) {                                    \
rb_check_argc(argc, 1);                                                    \
Klass *k = getPrivateData<Klass>(self);                                    \
type value;                                                                \
rb_##arg_fun##_arg(*argv, &value);                                         \
k->set##PropName(value);                                                   \
return *argv;                                                              \
}                                                                          \
RB_METHOD_GUARD_END

#define DEF_PROP_I(Klass, PropName)                                            \
DEF_PROP(Klass, int, PropName, int, rb_fix_new)

#define DEF_PROP_F(Klass, PropName)                                            \
DEF_PROP(Klass, double, PropName, float, rb_float_new)

#define DEF_PROP_B(Klass, PropName)                                            \
DEF_PROP(Klass, bool, PropName, bool, rb_bool_new)

#define INIT_PROP_BIND(Klass, PropName, prop_name_s)                           \
{                                                                            \
_rb_define_method(klass, prop_name_s, Klass##Get##PropName);               \
_rb_define_method(klass, prop_name_s "=", Klass##Set##PropName);           \
}

// --------------
// Wait for Graphics.update
// --------------
#if RAPI_FULL > 187
#define DEF_GFX_PROP_OBJ_REF(Klass, PropKlass, PropName, prop_iv)                  \
RB_METHOD(Klass##Get##PropName) {                                            \
RB_UNUSED_PARAM;                                                           \
return rb_iv_get(self, prop_iv);                                           \
}                                                                            \
RB_METHOD_GUARD(Klass##Set##PropName) {                                    \
RB_UNUSED_PARAM;                                                           \
rb_check_argc(argc, 1);                                                    \
Klass *k = getPrivateData<Klass>(self);                                    \
VALUE propObj = *argv;                                                     \
PropKlass *prop;                                                           \
if (NIL_P(propObj))                                                        \
prop = 0;                                                                \
else                                                                       \
prop = getPrivateDataCheck<PropKlass>(propObj, PropKlass##Type);         \
GFX_GUARD_EXC(k->set##PropName(prop);)                                         \
rb_iv_set(self, prop_iv, propObj);                                         \
return propObj;                                                            \
}                                                                          \
RB_METHOD_GUARD_END
#else
#define DEF_GFX_PROP_OBJ_REF(Klass, PropKlass, PropName, prop_iv)                  \
DEF_PROP_OBJ_REF(Klass, PropKlass, PropName, prop_iv)
#endif

/* Object property which is copied by value, not reference */
#if RAPI_FULL > 187
#define DEF_GFX_PROP_OBJ_VAL(Klass, PropKlass, PropName, prop_iv)                  \
RB_METHOD(Klass##Get##PropName) {                                            \
RB_UNUSED_PARAM;                                                           \
checkDisposed<Klass>(self);                                                \
return rb_iv_get(self, prop_iv);                                           \
}                                                                            \
RB_METHOD_GUARD(Klass##Set##PropName) {                                    \
rb_check_argc(argc, 1);                                                    \
Klass *k = getPrivateData<Klass>(self);                                    \
VALUE propObj = *argv;                                                     \
PropKlass *prop;                                                           \
prop = getPrivateDataCheck<PropKlass>(propObj, PropKlass##Type);           \
GFX_GUARD_EXC(k->set##PropName(*prop);)                                        \
return propObj;                                                            \
}                                                                          \
RB_METHOD_GUARD_END
#else
#define DEF_GFX_PROP_OBJ_VAL(Klass, PropKlass, PropName, prop_iv)                  \
DEF_PROP_OBJ_VAL(Klass, PropKlass, PropName, prop_iv)
#endif

#define DEF_GFX_PROP(Klass, type, PropName, arg_fun, value_fun)            \
RB_METHOD_GUARD(Klass##Get##PropName) {                                    \
RB_UNUSED_PARAM;                                                           \
Klass *k = getPrivateData<Klass>(self);                                    \
type value = 0;                                                            \
value = k->get##PropName();                                                \
return value_fun(value);                                                   \
}                                                                            \
RB_METHOD_GUARD_END                                                        \
RB_METHOD_GUARD(Klass##Set##PropName) {                                    \
rb_check_argc(argc, 1);                                                    \
Klass *k = getPrivateData<Klass>(self);                                    \
type value;                                                                \
rb_##arg_fun##_arg(*argv, &value);                                         \
GFX_GUARD_EXC(k->set##PropName(value);)                                        \
return *argv;                                                              \
}                                                                          \
RB_METHOD_GUARD_END

#define DEF_GFX_PROP_I(Klass, PropName)                                            \
DEF_GFX_PROP(Klass, int, PropName, int, rb_fix_new)

#define DEF_GFX_PROP_F(Klass, PropName)                                            \
DEF_GFX_PROP(Klass, double, PropName, float, rb_float_new)

#define DEF_GFX_PROP_B(Klass, PropName)                                            \
DEF_GFX_PROP(Klass, bool, PropName, bool, rb_bool_new)

#endif // BINDING_UTIL_H
