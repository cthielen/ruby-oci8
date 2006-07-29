/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 *  oranumber.c
 *
 * $Author$
 * $Date$
 *
 * Copyright (C) 2002-2005 KUBO Takehiro <kubo@jiubao.org>
 *
 * date and time between 4712 B.C. and 9999 A.D.
 */
#include "oci8.h"
#include "math.h"

static VALUE cOraNumber;

/* Member of ora_vnumber_t and ora_bind_handle_t - Internal format of NUMBER */
struct ora_number {
    unsigned char exponent;
    unsigned char mantissa[20];
};

/* OraNumber - Internal format of VARNUM */
struct ora_vnumber {
    unsigned char size;
    struct ora_number num;
};
typedef struct ora_vnumber ora_vnumber_t;

#define Get_Sign(on) ((on)->exponent & 0x80)
#define Get_Exp_Part(on) ((on)->exponent & 0x7F)
#define Get_Exponent(on) (Get_Sign(on) ? (Get_Exp_Part(on) - 65) : (~(Get_Exp_Part(on)) + 63))
#define Get_Mantissa_At(on, i) (Get_Sign(on) ? (on)->mantissa[i] - 1 : 101 - (on)->mantissa[i])

static ora_vnumber_t *get_ora_number(VALUE self)
{
    ora_vnumber_t *ovn;
    Data_Get_Struct(self, ora_vnumber_t, ovn);
    return ovn;
}

static VALUE ora_number_s_allocate(klass)
{
  ora_vnumber_t *ovn;
  return Data_Make_Struct(klass, ora_vnumber_t, NULL, xfree, ovn);
}

/*
=begin
--- OraNumber.new()
=end
*/
static VALUE ora_number_initialize(int argc, VALUE *argv, VALUE self)
{
    ora_vnumber_t *ovn = get_ora_number(self);

    ovn->size = 1;
    ovn->num.exponent = 0x80;
    memset(ovn->num.mantissa, 0, sizeof(ovn->num.mantissa));
    return Qnil;
}

static VALUE ora_number_initialize_copy(VALUE lhs, VALUE rhs)
{
    ora_vnumber_t *l, *r;

    /* ruby 1.8 */
    rb_obj_init_copy(lhs, rhs);
    Data_Get_Struct(lhs, ora_vnumber_t, l);
    Data_Get_Struct(rhs, ora_vnumber_t, r);
    memcpy(l, r, sizeof(ora_vnumber_t));
    return lhs;
}  

static VALUE ora_number_clone(VALUE self)
{
    VALUE obj = ora_number_s_allocate(CLASS_OF(self));
    return ora_number_initialize_copy(obj, self);
}

/*
=begin
--- OraNumber#to_i()
=end
*/
static VALUE ora_number_to_i(VALUE self)
{
    ora_vnumber_t *ovn = get_ora_number(self);
    unsigned char buf[ORA_NUMBER_BUF_SIZE];

    ora_number_to_str(buf, NULL, &(ovn->num), ovn->size);
    return rb_cstr2inum(buf, 10);
}

/*
=begin
--- OraNumber#to_f()
=end
*/
static VALUE ora_number_to_f(VALUE self)
{
    ora_vnumber_t *ovn = get_ora_number(self);
    unsigned char buf[ORA_NUMBER_BUF_SIZE];

    ora_number_to_str(buf, NULL, &(ovn->num), ovn->size);
    return rb_float_new(rb_cstr_to_dbl(buf, Qfalse));
}

/*
=begin
--- OraNumber#to_s()
=end
*/
static VALUE ora_number_to_s(VALUE self)
{
    ora_vnumber_t *ovn = get_ora_number(self);
    unsigned char buf[ORA_NUMBER_BUF_SIZE];
    size_t len;

    ora_number_to_str(buf, &len, &(ovn->num), ovn->size);
    return rb_str_new(buf, len);
}

static VALUE ora_number_uminus(VALUE self)
{
    ora_vnumber_t *ovn = get_ora_number(self);
    VALUE obj;
    int i;

    if (ovn->num.exponent == 0x80)
        return self;
    obj = ora_number_clone(self);
    ovn = get_ora_number(obj);
    ovn->num.exponent = ~(ovn->num.exponent);
    for (i = 0;i < ovn->size - 1;i++)
        ovn->num.mantissa[i] = 102 - ovn->num.mantissa[i];
    if (Get_Sign(&(ovn->num))) {
        if (ovn->size != 21 || ovn->num.mantissa[19] == 0x00) {
            ovn->size--;
        }
    } else {
        if (ovn->size != 21) {
            ovn->num.mantissa[ovn->size - 1] = 102;
            ovn->size++;
        }
    }
    return obj;
}

static VALUE ora_number_dump(int argc, VALUE *argv, VALUE self)
{
    ora_vnumber_t *ovn = get_ora_number(self);
    return rb_str_new((const char*)ovn, sizeof(ora_vnumber_t));
}  

static VALUE ora_number_s_load(VALUE klass, VALUE str)
{
    ora_vnumber_t *ovn;
    VALUE obj;

    StringValue(str);
    if (RSTRING(str)->len != sizeof(ora_vnumber_t)) {
        rb_raise(rb_eTypeError, "marshaled OraNumber format differ");
    }
    obj = ora_number_s_allocate(klass);
    ovn = get_ora_number(obj);
    memcpy(ovn, RSTRING(str)->ptr, sizeof(ora_vnumber_t));
    return obj;
}  

/*
 * bind_oranumber
 */
typedef struct {
    oci8_bind_t base;
    ora_number_t on;
} oci8_bind_oranumber_t;

static VALUE bind_oranumber_get(oci8_bind_t *bb)
{
    ora_vnumber_t *ovn;
    VALUE obj = Data_Make_Struct(cOraNumber, ora_vnumber_t, NULL, xfree, ovn);
    ovn->size = bb->rlen;
    memcpy(&(ovn->num), bb->valuep, sizeof(ora_number_t));
    return obj;
}

static void bind_oranumber_set(oci8_bind_t *bb, VALUE val)
{
    ora_vnumber_t *ovn;
    Check_Object(val, cOraNumber);
    Data_Get_Struct(val, ora_vnumber_t, ovn);
    bb->rlen = ovn->size;
    memcpy(bb->valuep, &ovn->num, sizeof(ora_number_t));
}

static void bind_oranumber_init(oci8_bind_t *bb, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_oranumber_t *bo = (oci8_bind_oranumber_t *)bb;
    bb->valuep = &bo->on;
    bb->value_sz = sizeof(ora_number_t);
}

static oci8_bind_class_t bind_oranumber_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_oranumber_t)
    },
    bind_oranumber_get,
    bind_oranumber_set,
    bind_oranumber_init,
    SQLT_NUM
};

void Init_ora_number(void)
{
    cOraNumber = rb_define_class("OraNumber", rb_cObject);

    rb_define_alloc_func(cOraNumber, ora_number_s_allocate);
    rb_define_method(cOraNumber, "initialize", ora_number_initialize, -1);
    rb_define_method(cOraNumber, "initialize_copy", ora_number_initialize_copy, 1);
    rb_define_method(cOraNumber, "to_i", ora_number_to_i, 0);
    rb_define_method(cOraNumber, "to_f", ora_number_to_f, 0);
    rb_define_method(cOraNumber, "to_s", ora_number_to_s, 0);
    rb_define_method(cOraNumber, "-@", ora_number_uminus, 0);
    rb_define_method(cOraNumber, "_dump", ora_number_dump, -1);
    rb_define_singleton_method(cOraNumber, "_load", ora_number_s_load, 1);

    oci8_define_bind_class("OraNumber", &bind_oranumber_class);
}

void ora_number_to_str(unsigned char *buf, size_t *lenp, ora_number_t *on, unsigned char size)
{
    int exponent;
    int len = 0;
    int mentissa_size;
    int i, j;

    if (on->exponent == 0x80) {
        buf[0] = '0';
        buf[1] = '\0';
        if (lenp != NULL)
            *lenp = 1;
        return;
    }

    if (Get_Sign(on)) {
        mentissa_size = size - 1;
    } else {
        if (size == 21 && on->mantissa[19] != 102)
            mentissa_size = 20;
        else
            mentissa_size = size - 2;
        buf[len++] = '-';
    }
    exponent = Get_Exponent(on);
    if (exponent < 0) {
        buf[len++] = '0';
        buf[len++] = '.';
        for (i = exponent * 2;i < -2;i++)
            buf[len++] = '0';
        for (i = 0;i < mentissa_size;i++) {
            j = Get_Mantissa_At(on, i);
            buf[len++] = j / 10 + '0';
            if (i != mentissa_size - 1 || j % 10 != 0)
                buf[len++] = j % 10 + '0';
        }
    } else {
        for (i = 0;i < mentissa_size;i++) {
            j = Get_Mantissa_At(on, i);
            if (i == exponent + 1) {
                buf[len++] = '.';
            }
            if (i != 0 || j / 10 != 0)
                buf[len++] = j / 10 + '0';
            if ((i < exponent + 1) /* integer part */
                || (i != mentissa_size - 1 || j % 10 != 0)  /* decimal fraction */)
                buf[len++] = j % 10 + '0';
        }
        for (;i <= exponent;i++) {
            buf[len++] = '0';
            buf[len++] = '0';
        }
    }
    buf[len] = '\0';
    if (lenp != NULL)
        *lenp = len;
}
