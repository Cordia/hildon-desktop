#ifndef TIDYBLUR_H_
#define TIDYBLUR_H_

#include <clutter/clutter-group.h>
#include <clutter/clutter-types.h>
#include <cogl/cogl.h>

G_BEGIN_DECLS

#define TIDY_TYPE_BLUR_GROUP                  (tidy_blur_group_get_type ())
#define TIDY_BLUR_GROUP(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), TIDY_TYPE_BLUR_GROUP, TidyBlurGroup))
#define TIDY_IS_BLUR_GROUP(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TIDY_TYPE_BLUR_GROUP))
#define TIDY_BLUR_GROUP_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), TIDY_TYPE_BLUR_GROUP, TidyBlurGroupClass))
#define TIDY_IS_BLUR_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), TIDY_TYPE_BLUR_GROUP))
#define TIDY_BLUR_GROUP_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), TIDY_TYPE_BLUR_GROUP, TidyBlurGroupClass))


typedef struct _TidyBlurGroup         TidyBlurGroup;
typedef struct _TidyBlurGroupClass    TidyBlurGroupClass;
typedef struct _TidyBlurGroupPrivate  TidyBlurGroupPrivate;

struct _TidyBlurGroup
{
  ClutterGroup          parent;

  TidyBlurGroupPrivate  *priv;
};

struct _TidyBlurGroupClass
{
  /*< private >*/
  ClutterGroupClass parent_class;

  void (*overridden_paint)(ClutterActor *actor);
};


GType tidy_blur_group_get_type (void) G_GNUC_CONST;
ClutterActor *tidy_blur_group_new (void);

void tidy_blur_group_set_blur(ClutterActor *blur_group, float blur);
void tidy_blur_group_set_saturation(ClutterActor *blur_group, float saturation);
void tidy_blur_group_set_brightness(ClutterActor *blur_group, float brightness);
void tidy_blur_group_set_zoom(ClutterActor *blur_group, float zoom);
float tidy_blur_group_get_zoom(ClutterActor *blur_group);
gboolean tidy_blur_group_source_buffered(ClutterActor *blur_group);

void tidy_blur_group_set_use_alpha(ClutterActor *blur_group, gboolean alpha);
void tidy_blur_group_set_use_mirror(ClutterActor *blur_group, gboolean mirror);
void tidy_blur_group_set_source_changed(ClutterActor *blur_group);


G_END_DECLS


#endif /*TIDYBLUR_H_*/
