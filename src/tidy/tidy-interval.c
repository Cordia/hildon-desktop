#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "tidy-interval.h"

enum
{
  PROP_0,

  PROP_VALUE_TYPE
};

#define TIDY_INTERVAL_GET_PRIVATE(obj)  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_INTERVAL, TidyIntervalPrivate))

struct _TidyIntervalPrivate
{
  GType value_type;

  GValue *values;
};

G_DEFINE_TYPE (TidyInterval, tidy_interval, G_TYPE_INITIALLY_UNOWNED);

static void
tidy_interval_finalize (GObject *gobject)
{
  TidyIntervalPrivate *priv = TIDY_INTERVAL (gobject)->priv;

  g_value_unset (&priv->values[0]);
  g_value_unset (&priv->values[1]);

  g_free (priv->values);
}

static void
tidy_interval_set_property (GObject      *gobject,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  TidyIntervalPrivate *priv = TIDY_INTERVAL_GET_PRIVATE (gobject);

  switch (prop_id)
    {
    case PROP_VALUE_TYPE:
      priv->value_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_interval_get_property (GObject    *gobject,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  TidyIntervalPrivate *priv = TIDY_INTERVAL_GET_PRIVATE (gobject);

  switch (prop_id)
    {
    case PROP_VALUE_TYPE:
      g_value_set_gtype (value, priv->value_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_interval_class_init (TidyIntervalClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (TidyIntervalPrivate));

  gobject_class->set_property = tidy_interval_set_property,
  gobject_class->get_property = tidy_interval_get_property;
  gobject_class->finalize = tidy_interval_finalize;

  pspec = g_param_spec_gtype ("value-type",
                              "Value Type",
                              "The type of the values in the interval",
                              G_TYPE_NONE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_VALUE_TYPE, pspec);
}

static void
tidy_interval_init (TidyInterval *self)
{
  TidyIntervalPrivate *priv;

  self->priv = priv = TIDY_INTERVAL_GET_PRIVATE (self);

  priv->value_type = G_TYPE_INVALID;
  priv->values = g_malloc0 (sizeof (GValue) * 2);
}

static void
tidy_interval_set_interval_valist (TidyInterval *interval,
                                    va_list        var_args)
{
  GType gtype = interval->priv->value_type;
  GValue value = { 0, };
  gchar *error;

  /* initial value */
  g_value_init (&value, gtype);
  G_VALUE_COLLECT (&value, var_args, 0, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);

      /* we leak the value here as it might not be in a valid state
       * given the error and calling g_value_unset() might lead to
       * undefined behaviour
       */
      g_free (error);
      return;
    }

  tidy_interval_set_initial_value (interval, &value);
  g_value_unset (&value);

  /* final value */
  g_value_init (&value, gtype);
  G_VALUE_COLLECT (&value, var_args, 0, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);

      /* see above */
      g_free (error);
      return;
    }

  tidy_interval_set_final_value (interval, &value);
  g_value_unset (&value);
}

static void
tidy_interval_get_interval_valist (TidyInterval *interval,
                                   va_list        var_args)
{
  GType gtype = interval->priv->value_type;
  GValue value = { 0, };
  gchar *error;

  /* initial value */
  g_value_init (&value, gtype);
  tidy_interval_get_initial_value (interval, &value);
  G_VALUE_LCOPY (&value, var_args, 0, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);
      g_free (error);
      g_value_unset (&value);
      return;
    }

  g_value_unset (&value);

  /* final value */
  g_value_init (&value, gtype);
  tidy_interval_get_final_value (interval, &value);
  G_VALUE_LCOPY (&value, var_args, 0, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);
      g_free (error);
      g_value_unset (&value);
      return;
    }

  g_value_unset (&value);
}

TidyInterval *
tidy_interval_new (GType gtype,
                   ...)
{
  TidyInterval *retval;
  va_list args;

  g_return_val_if_fail (gtype != G_TYPE_INVALID, NULL);

  retval = g_object_new (TIDY_TYPE_INTERVAL, "value-type", gtype, NULL);

  va_start (args, gtype);
  tidy_interval_set_interval_valist (retval, args);
  va_end (args);

  return retval;
}

TidyInterval *
tidy_interval_new_with_values (GType         gtype,
                               const GValue *initial,
                               const GValue *final)
{
  TidyInterval *retval;

  g_return_val_if_fail (gtype != G_TYPE_INVALID, NULL);
  g_return_val_if_fail (initial != NULL, NULL);
  g_return_val_if_fail (final != NULL, NULL);
  g_return_val_if_fail (G_VALUE_TYPE (initial) == gtype, NULL);
  g_return_val_if_fail (G_VALUE_TYPE (final) == gtype, NULL);

  retval = g_object_new (TIDY_TYPE_INTERVAL, "value-type", gtype, NULL);

  tidy_interval_set_initial_value (retval, initial);
  tidy_interval_set_final_value (retval, final);

  return retval;
}

TidyInterval *
tidy_interval_clone (TidyInterval *interval)
{
  TidyInterval *retval;
  GType gtype;

  g_return_val_if_fail (TIDY_IS_INTERVAL (interval), NULL);
  g_return_val_if_fail (interval->priv->value_type != G_TYPE_INVALID, NULL);

  gtype = interval->priv->value_type;
  retval = g_object_new (TIDY_TYPE_INTERVAL, "value-type", gtype, NULL);

  tidy_interval_set_initial_value (retval,
                                   tidy_interval_peek_initial_value (interval));
  tidy_interval_set_final_value (retval,
                                 tidy_interval_peek_final_value (interval));

  return retval;
}

GType
tidy_interval_get_value_type (TidyInterval *interval)
{
  g_return_val_if_fail (TIDY_IS_INTERVAL (interval), G_TYPE_INVALID);

  return interval->priv->value_type;
}

void
tidy_interval_set_initial_value (TidyInterval *interval,
                                 const GValue  *value)
{
  TidyIntervalPrivate *priv;

  g_return_if_fail (TIDY_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  priv = interval->priv;

  if (G_IS_VALUE (&priv->values[0]))
    g_value_unset (&priv->values[0]);

  g_value_init (&priv->values[0], priv->value_type);
  g_value_copy (value, &priv->values[0]);
}

void
tidy_interval_get_initial_value (TidyInterval *interval,
                                 GValue        *value)
{
  TidyIntervalPrivate *priv;

  g_return_if_fail (TIDY_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  priv = interval->priv;

  g_value_copy (&priv->values[0], value);
}

GValue *
tidy_interval_peek_initial_value (TidyInterval *interval)
{
  g_return_val_if_fail (TIDY_IS_INTERVAL (interval), NULL);

  return interval->priv->values;
}

void
tidy_interval_set_final_value (TidyInterval *interval,
                               const GValue  *value)
{
  TidyIntervalPrivate *priv;

  g_return_if_fail (TIDY_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  priv = interval->priv;

  if (G_IS_VALUE (&priv->values[1]))
    g_value_unset (&priv->values[1]);

  g_value_init (&priv->values[1], priv->value_type);
  g_value_copy (value, &priv->values[1]);
}

void
tidy_interval_get_final_value (TidyInterval *interval,
                               GValue        *value)
{
  TidyIntervalPrivate *priv;

  g_return_if_fail (TIDY_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  priv = interval->priv;

  g_value_copy (&priv->values[1], value);
}

GValue *
tidy_interval_peek_final_value (TidyInterval *interval)
{
  g_return_val_if_fail (TIDY_IS_INTERVAL (interval), NULL);

  return interval->priv->values + 1;
}

void
tidy_interval_set_interval (TidyInterval *interval,
                            ...)
{
  va_list args;

  g_return_if_fail (TIDY_IS_INTERVAL (interval));
  g_return_if_fail (interval->priv->value_type != G_TYPE_INVALID);

  va_start (args, interval);
  tidy_interval_set_interval_valist (interval, args);
  va_end (args);
}

void
tidy_interval_get_interval (TidyInterval *interval,
                            ...)
{
  va_list args;

  g_return_if_fail (TIDY_IS_INTERVAL (interval));
  g_return_if_fail (interval->priv->value_type != G_TYPE_INVALID);

  va_start (args, interval);
  tidy_interval_get_interval_valist (interval, args);
  va_end (args);
}
