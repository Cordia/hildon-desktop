#ifndef __TIDY_INTERVAL_H__
#define __TIDY_INTERVAL_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TIDY_TYPE_INTERVAL              (tidy_interval_get_type ())
#define TIDY_INTERVAL(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), TIDY_TYPE_INTERVAL, TidyInterval))
#define TIDY_IS_INTERVAL(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TIDY_TYPE_INTERVAL))
#define TIDY_INTERVAL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), TIDY_TYPE_INTERVAL, TidyIntervalClass))
#define TIDY_IS_INTERVAL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), TIDY_TYPE_INTERVAL))
#define TIDY_INTERVAL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), TIDY_TYPE_INTERVAL, TidyIntervalClass))

typedef struct _TidyInterval            TidyInterval;
typedef struct _TidyIntervalPrivate     TidyIntervalPrivate;
typedef struct _TidyIntervalClass       TidyIntervalClass;

struct _TidyInterval
{
  /*< private >*/
  GInitiallyUnowned parent_instance;

  TidyIntervalPrivate *priv;
};

struct _TidyIntervalClass
{
  GInitiallyUnownedClass parent_class;
};

GType         tidy_interval_get_type           (void) G_GNUC_CONST;

TidyInterval *tidy_interval_new                (GType         gtype,
                                                  ...);
TidyInterval *tidy_interval_new_with_values    (GType         gtype,
                                                const GValue *initial,
                                                const GValue *final);

TidyInterval *tidy_interval_clone              (TidyInterval *interval);

GType         tidy_interval_get_value_type     (TidyInterval *interval);
void          tidy_interval_set_initial_value  (TidyInterval *interval,
                                                const GValue *value);
void          tidy_interval_get_initial_value  (TidyInterval *interval,
                                                GValue       *value);
GValue *      tidy_interval_peek_initial_value (TidyInterval *interval);
void          tidy_interval_set_final_value    (TidyInterval *interval,
                                                const GValue *value);
void          tidy_interval_get_final_value    (TidyInterval *interval,
                                                GValue       *value);
GValue *      tidy_interval_peek_final_value   (TidyInterval *interval);

void          tidy_interval_set_interval       (TidyInterval *interval,
                                                ...);
void          tidy_interval_get_interval       (TidyInterval *interval,
                                                ...);

G_END_DECLS

#endif /* __TIDY_INTERVAL_H__ */
