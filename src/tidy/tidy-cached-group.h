#ifndef TIDYBLUR_H_
#define TIDYBLUR_H_

#include <clutter/clutter-group.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define TIDY_TYPE_CACHED_GROUP                  (tidy_cached_group_get_type ())
#define TIDY_CACHED_GROUP(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), TIDY_TYPE_CACHED_GROUP, TidyCachedGroup))
#define TIDY_IS_CACHED_GROUP(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TIDY_TYPE_CACHED_GROUP))
#define TIDY_CACHED_GROUP_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), TIDY_TYPE_CACHED_GROUP, TidyCachedGroupClass))
#define TIDY_IS_CACHED_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), TIDY_TYPE_CACHED_GROUP))
#define TIDY_CACHED_GROUP_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), TIDY_TYPE_CACHED_GROUP, TidyCachedGroupClass))


typedef struct _TidyCachedGroup         TidyCachedGroup;
typedef struct _TidyCachedGroupClass    TidyCachedGroupClass;
typedef struct _TidyCachedGroupPrivate  TidyCachedGroupPrivate;

struct _TidyCachedGroup
{
  ClutterGroup          parent;

  TidyCachedGroupPrivate  *priv;
};

struct _TidyCachedGroupClass
{
  ClutterGroupClass parent_class;
};


GType tidy_cached_group_get_type (void) G_GNUC_CONST;
ClutterActor *tidy_cached_group_new (void);

void tidy_cached_group_set_render_cache(ClutterActor *cached_group, float amount);
void tidy_cached_group_changed(ClutterActor *cached_group);


G_END_DECLS


#endif /*TIDYBLUR_H_*/
