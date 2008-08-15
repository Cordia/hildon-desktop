
#ifndef ___tidy_marshal_MARSHAL_H__
#define ___tidy_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:OBJECT (./tidy-marshal.list:1) */
#define _tidy_marshal_VOID__OBJECT	g_cclosure_marshal_VOID__OBJECT

/* VOID:VOID (./tidy-marshal.list:2) */
#define _tidy_marshal_VOID__VOID	g_cclosure_marshal_VOID__VOID

/* VOID:PARAM (./tidy-marshal.list:3) */
#define _tidy_marshal_VOID__PARAM	g_cclosure_marshal_VOID__PARAM

/* VOID:UINT (./tidy-marshal.list:4) */
#define _tidy_marshal_VOID__UINT	g_cclosure_marshal_VOID__UINT

/* VOID:UINT,UINT (./tidy-marshal.list:5) */
extern void _tidy_marshal_VOID__UINT_UINT (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);

/* VOID:OBJECT,OBJECT (./tidy-marshal.list:6) */
extern void _tidy_marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);

G_END_DECLS

#endif /* ___tidy_marshal_MARSHAL_H__ */

