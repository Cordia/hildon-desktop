#ifndef __TIDY_ANIMATION_H__
#define __TIDY_ANIMATION_H__

#include <clutter/clutter-actor.h>

#include "tidy-interval.h"

G_BEGIN_DECLS

#define TIDY_TYPE_ANIMATION             (tidy_animation_get_type ())
#define TIDY_ANIMATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TIDY_TYPE_ANIMATION, TidyAnimation))
#define TIDY_IS_ANIMATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TIDY_TYPE_ANIMATION))
#define TIDY_ANIMATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TIDY_TYPE_ANIMATION, TidyAnimationClass))
#define TIDY_IS_ANIMATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TIDY_TYPE_ANIMATION))
#define TIDY_ANIMATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TIDY_TYPE_ANIMATION, TidyAnimationClass))

typedef struct _TidyAnimation           TidyAnimation;
typedef struct _TidyAnimationPrivate    TidyAnimationPrivate;
typedef struct _TidyAnimationClass      TidyAnimationClass;

typedef enum {
  TIDY_LINEAR,
  TIDY_SINE
} TidyAnimationMode;

struct _TidyAnimation
{
  GInitiallyUnowned parent_instance;

  TidyAnimationPrivate *priv;
};

struct _TidyAnimationClass
{
  GInitiallyUnownedClass parent_class;

  void (* completed) (TidyAnimation *animation);
};

GType             tidy_animation_get_type        (void) G_GNUC_CONST;

TidyAnimation *   tidy_animation_new             (void);
void              tidy_animation_set_actor       (TidyAnimation     *animation,
                                                  ClutterActor       *actor);
ClutterActor *    tidy_animation_get_actor       (TidyAnimation     *animation);
void              tidy_animation_set_mode        (TidyAnimation     *animation,
                                                  TidyAnimationMode  mode);
TidyAnimationMode tidy_animation_get_mode        (TidyAnimation     *animation);
void              tidy_animation_set_duration    (TidyAnimation     *animation,
                                                  gint                 msecs);
guint             tidy_animation_get_duration    (TidyAnimation     *animation);
void              tidy_animation_set_loop        (TidyAnimation     *animation,
                                                  gboolean             loop);
gboolean          tidy_animation_get_loop        (TidyAnimation     *animation);
void              tidy_animation_bind_property   (TidyAnimation     *animation,
                                                  const gchar        *property_name,
                                                  TidyInterval      *interval);
gboolean          tidy_animation_has_property    (TidyAnimation     *animation,
                                                  const gchar        *property_name);
void              tidy_animation_unbind_property (TidyAnimation     *animation,
                                                  const gchar        *property_name);

void              tidy_animation_start           (TidyAnimation     *animation);
void              tidy_animation_stop            (TidyAnimation     *animation);

/* wrapper */
TidyAnimation *   tidy_actor_animate             (ClutterActor       *actor,
                                                  TidyAnimationMode  mode,
                                                  guint               duration,
                                                  const gchar        *first_property_name,
                                                  ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* __TIDY_ANIMATION_H__ */
