#include <glib-object.h>

#define I_(str) (g_intern_static_string ((str)))

#define HD_PARAM_READWRITE      (G_PARAM_READWRITE | \
                                 G_PARAM_STATIC_NICK | \
                                 G_PARAM_STATIC_NAME | \
                                 G_PARAM_STATIC_BLURB)
#define HD_PARAM_READABLE       (G_PARAM_READABLE | \
                                 G_PARAM_STATIC_NICK | \
                                 G_PARAM_STATIC_NAME | \
                                 G_PARAM_STATIC_BLURB)
#define HD_PARAM_WRITABLE       (G_PARAM_WRITABLE | \
                                 G_PARAM_STATIC_NICK | \
                                 G_PARAM_STATIC_NAME | \
                                 G_PARAM_STATIC_BLURB)
