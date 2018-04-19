/*
 * QFloat Module
 *
 * Copyright (C) 2009 Red Hat Inc.
 *
 * Authors:
 *  Luiz Capitulino <lcapitulino@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Copyright IBM, Corp. 2009
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#include "qfloat.h"
#include "qobject.h"
#include "qemu-common.h"

static void qfloat_destroy_obj(QObject *obj);

#ifdef __GNUC__

static const QType qfloat_type = {
    .code = QTYPE_QFLOAT,
    .destroy = qfloat_destroy_obj,
};

#else

static const QType qfloat_type = {
    QTYPE_QFLOAT,
    qfloat_destroy_obj,
};

#endif

/**
 * qfloat_from_int(): Create a new QFloat from a float
 *
 * Return strong reference.
 */
QFloat *qfloat_from_double(double value)
{
    QFloat *qf;

    qf = qemu_malloc(sizeof(*qf));
    qf->value = value;
    QOBJECT_INIT(qf, &qfloat_type);

    return qf;
}

/**
 * qfloat_get_double(): Get the stored float
 */
double qfloat_get_double(const QFloat *qf)
{
    return qf->value;
}

/**
 * qobject_to_qfloat(): Convert a QObject into a QFloat
 */
QFloat *qobject_to_qfloat(const QObject *obj)
{
    if (qobject_type(obj) != QTYPE_QFLOAT)
        return NULL;

    return container_of(obj, QFloat, base);
}

/**
 * qfloat_destroy_obj(): Free all memory allocated by a
 * QFloat object
 */
static void qfloat_destroy_obj(QObject *obj)
{
    assert(obj != NULL);
    qemu_free(qobject_to_qfloat(obj));
}