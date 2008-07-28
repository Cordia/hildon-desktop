#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>

#include <clutter/clutter.h>

#include "hd-launcher-item.h"
#include "hd-task-launcher.h"

#define I_(str) (g_intern_static_string ((str)))

GType
hd_launcher_item_type_get_type (void)
{
  static GType gtype = 0;

  if (G_UNLIKELY (gtype == 0))
    {
      static GEnumValue values[] = {
        { HD_APPLICATION_LAUNCHER, "HD_APPLICATION_LAUNCHER", "application" },
        { HD_SECTION_LAUNCHER, "HD_SECTION_LAUNCHER", "section" },
        { 0, NULL, NULL }
      };

      gtype = g_enum_register_static (I_("HdLauncherItemType"), values);
    }

  return gtype;
}

/* HdLauncherItem layout:
 *
 *              _  <padding.right>
 *             | 
 *             v
 * +------------+
 * |            | <- <padding.top>
 * |   +----+   |
 * |   |xxxx|   |
 * |   |xxxx|   |  icon
 * |   |xxxx|   |
 * |   +----+   |
 * |            | <- <spacing>
 * |  xxxxxxxx  |
 * |  xxxxxxxx  |  label
 * |  xxxxxxxx  |
 * |            | <- <padding.bottom>
 * +------------+
 *  ^
 *  |_ <padding.left>
 */

#define HD_LAUNCHER_ITEM_GET_PRIVATE(obj)       (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_LAUNCHER_ITEM, HdLauncherItemPrivate))

struct _HdLauncherItemPrivate
{
  HdLauncherItemType item_type;

  ClutterEffectTemplate *tmpl;

  ClutterActor *label;
  ClutterActor *icon;

  HdLauncherPadding padding;
  ClutterUnit spacing;

  guint is_pressed : 1;
};

enum
{
  PROP_0,

  PROP_LAUNCHER_TYPE
};

enum
{
  CLICKED,

  LAST_SIGNAL
};

static guint launcher_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_ABSTRACT_TYPE (HdLauncherItem, hd_launcher_item, CLUTTER_TYPE_ACTOR);

static ClutterActor *
hd_launcher_item_get_icon_unimplemented (HdLauncherItem *item)
{
  g_critical ("%s: HdLauncherItem of type `%s' does not implement "
              "the HdLauncherItemClass::get_icon() virtual function",
              G_STRLOC,
              G_OBJECT_TYPE_NAME (item));
  return NULL;
}

static ClutterActor *
hd_launcher_item_get_label_unimplemented (HdLauncherItem *item)
{
  g_critical ("%s: HdLauncherItem of type `%s' does not implement "
              "the HdLauncherItemClass::get_label() virtual function",
              G_STRLOC,
              G_OBJECT_TYPE_NAME (item));
  return NULL;
}

static void
hd_launcher_item_get_preferred_width (ClutterActor *actor,
                                      ClutterUnit   for_height,
                                      ClutterUnit  *min_width_p,
                                      ClutterUnit  *natural_width_p)
{
  HdLauncherItemPrivate *priv = HD_LAUNCHER_ITEM (actor)->priv;
  ClutterUnit label_width = 0;

  /* our size depends on the label size */
  clutter_actor_get_preferred_width (priv->label, for_height,
                                     NULL,
                                     &label_width);

  /* at least we require the label to be visible */
  if (min_width_p)
    *min_width_p = label_width;

  if (natural_width_p)
    *natural_width_p = label_width + priv->padding.left + priv->padding.right;
}

static void
hd_launcher_item_get_preferred_height (ClutterActor *actor,
                                       ClutterUnit   for_width,
                                       ClutterUnit  *min_height_p,
                                       ClutterUnit  *natural_height_p)
{
  HdLauncherItemPrivate *priv = HD_LAUNCHER_ITEM (actor)->priv;
  ClutterUnit icon_height, label_height;

  clutter_actor_get_preferred_height (priv->icon, for_width,
                                      NULL,
                                      &icon_height);
  clutter_actor_get_preferred_height (priv->label, for_width,
                                      NULL,
                                      &label_height);

  if (min_height_p)
    *min_height_p = icon_height + label_height;

  if (natural_height_p)
    *natural_height_p = priv->padding.top
                      + icon_height
                      + priv->spacing
                      + label_height
                      + priv->padding.bottom;
}

static void
hd_launcher_item_allocate (ClutterActor          *actor,
                           const ClutterActorBox *box,
                           gboolean               origin_changed)
{
  HdLauncherItemPrivate *priv = HD_LAUNCHER_ITEM (actor)->priv;
  ClutterActorClass *parent_class;
  ClutterUnit icon_width, icon_height;
  ClutterUnit label_width, label_height;

  /* chain up to get the allocation stored */
  parent_class = CLUTTER_ACTOR_CLASS (hd_launcher_item_parent_class);
  parent_class->allocate (actor, box, origin_changed);

  label_width = box->x2 - box->x1
              - priv->padding.left
              - priv->padding.right;

  {
    ClutterActorBox icon_box = { 0, };

    clutter_actor_get_preferred_size (priv->icon,
                                      NULL, NULL,
                                      &icon_width, &icon_height);

    icon_box.x1 = (label_width - icon_width) / 2;
    icon_box.y1 = priv->padding.top;
    icon_box.x2 = icon_box.x1 + icon_width;
    icon_box.y2 = icon_box.y1 + icon_height;

    clutter_actor_allocate (priv->icon, &icon_box, origin_changed);
  }

  {
    ClutterActorBox label_box = { 0, };

    clutter_actor_get_preferred_height (priv->label, label_width,
                                        NULL,
                                        &label_height);

    label_box.x1 = priv->padding.left;
    label_box.y1 = priv->padding.top
                 + icon_height
                 + priv->spacing;
    label_box.x2 = label_box.x1 + label_width;
    label_box.y2 = label_box.y1 + label_height;

    clutter_actor_allocate (priv->label, &label_box, origin_changed);
  }
}

static void
hd_launcher_item_paint (ClutterActor *actor)
{
  HdLauncherItemPrivate *priv = HD_LAUNCHER_ITEM (actor)->priv;

  if (priv->label && CLUTTER_ACTOR_IS_VISIBLE (priv->label))
    clutter_actor_paint (priv->label);

  if (priv->icon && CLUTTER_ACTOR_IS_VISIBLE (priv->icon))
    clutter_actor_paint (priv->icon);
}

static void
hd_launcher_item_pick (ClutterActor       *actor,
                       const ClutterColor *pick_color)
{
  HdLauncherItemPrivate *priv = HD_LAUNCHER_ITEM (actor)->priv;

  CLUTTER_ACTOR_CLASS (hd_launcher_item_parent_class)->pick (actor, pick_color);

  if (priv->label && CLUTTER_ACTOR_IS_VISIBLE (priv->label))
    clutter_actor_paint (priv->label);

  if (priv->icon && CLUTTER_ACTOR_IS_VISIBLE (priv->icon))
    clutter_actor_paint (priv->icon);
}

static void
hd_launcher_item_show (ClutterActor *actor)
{
  HdLauncherItemPrivate *priv = HD_LAUNCHER_ITEM (actor)->priv;

  clutter_actor_show (priv->icon);
  clutter_actor_show (priv->label);

  CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_MAPPED);
}

static void
hd_launcher_item_hide (ClutterActor *actor)
{
  HdLauncherItemPrivate *priv = HD_LAUNCHER_ITEM (actor)->priv;

  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_MAPPED);

  clutter_actor_hide (priv->icon);
  clutter_actor_hide (priv->label);
}

static gboolean
hd_launcher_item_button_press (ClutterActor       *actor,
                               ClutterButtonEvent *event)
{
  if (event->button == 1)
    {
      HdLauncherItem *item = HD_LAUNCHER_ITEM (actor);
      HdLauncherItemPrivate *priv = item->priv;
      HdLauncherItemClass *klass = HD_LAUNCHER_ITEM_GET_CLASS (item);

      priv->is_pressed = TRUE;

      if (klass->pressed)
        klass->pressed (item);

      return TRUE;
    }

  return FALSE;
}

static gboolean
hd_launcher_item_button_release (ClutterActor       *actor,
                                 ClutterButtonEvent *event)
{
  if (event->button == 1)
    {
      HdLauncherItem *item = HD_LAUNCHER_ITEM (actor);
      HdLauncherItemPrivate *priv = item->priv;
      HdLauncherItemClass *klass = HD_LAUNCHER_ITEM_GET_CLASS (item);

      if (!priv->is_pressed)
        return FALSE;

      priv->is_pressed = FALSE;

      if (klass->released)
        klass->released (item);

      g_print ("released: %s", G_OBJECT_TYPE_NAME (item));

      g_signal_emit (item, launcher_signals[CLICKED], 0);

      return TRUE;
    }

  return FALSE;
}

static void
hd_launcher_item_finalize (GObject *gobject)
{
  HdLauncherItemPrivate *priv = HD_LAUNCHER_ITEM_GET_PRIVATE (gobject);

  g_object_unref (priv->tmpl);

  clutter_actor_destroy (priv->label);
  clutter_actor_destroy (priv->icon);

  G_OBJECT_CLASS (hd_launcher_item_parent_class)->finalize (gobject);
}

static void
hd_launcher_item_constructed (GObject *gobject)
{
  HdLauncherItem *item = HD_LAUNCHER_ITEM (gobject);
  HdLauncherItemPrivate *priv = item->priv;

  priv->icon = hd_launcher_item_get_icon (item);
  g_assert (CLUTTER_IS_ACTOR (priv->icon));
  clutter_actor_set_parent (priv->icon, CLUTTER_ACTOR (item));

  priv->label = hd_launcher_item_get_label (item);
  g_assert (CLUTTER_IS_ACTOR (priv->label));
  clutter_actor_set_parent (priv->label, CLUTTER_ACTOR (item));
}

static void
hd_launcher_item_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HdLauncherItemPrivate *priv = HD_LAUNCHER_ITEM_GET_PRIVATE (gobject);

  switch (prop_id)
    {
    case PROP_LAUNCHER_TYPE:
      priv->item_type = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
hd_launcher_item_class_init (HdLauncherItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (HdLauncherItemPrivate));

  klass->get_icon  = hd_launcher_item_get_icon_unimplemented;
  klass->get_label = hd_launcher_item_get_label_unimplemented;

  gobject_class->constructed  = hd_launcher_item_constructed;
  gobject_class->set_property = hd_launcher_item_set_property;
  gobject_class->finalize     = hd_launcher_item_finalize;

  actor_class->get_preferred_width  = hd_launcher_item_get_preferred_width;
  actor_class->get_preferred_height = hd_launcher_item_get_preferred_height;
  actor_class->allocate             = hd_launcher_item_allocate;
  actor_class->paint                = hd_launcher_item_paint;
  actor_class->pick                 = hd_launcher_item_pick;
  actor_class->button_press_event   = hd_launcher_item_button_press;
  actor_class->button_release_event = hd_launcher_item_button_release;
  actor_class->show                 = hd_launcher_item_show;
  actor_class->hide                 = hd_launcher_item_hide;

  pspec = g_param_spec_enum ("launcher-type",
                             "Launcher Type",
                             "Type of the launcher",
                             HD_TYPE_LAUNCHER_ITEM_TYPE,
                             HD_APPLICATION_LAUNCHER,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);
  g_object_class_install_property (gobject_class, PROP_LAUNCHER_TYPE, pspec);

  launcher_signals[CLICKED] =
    g_signal_new (I_("clicked"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (HdLauncherItemClass, clicked),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hd_launcher_item_init (HdLauncherItem *item)
{
  HdLauncherItemPrivate *priv;

  item->priv = priv = HD_LAUNCHER_ITEM_GET_PRIVATE (item);

  priv->item_type = HD_APPLICATION_LAUNCHER;

  priv->padding.top = priv->padding.bottom = 0;
  priv->padding.right = priv->padding.left = 0;

  priv->spacing = CLUTTER_UNITS_FROM_DEVICE (2);

  priv->tmpl = clutter_effect_template_new_for_duration (250, CLUTTER_ALPHA_RAMP);

  clutter_actor_set_reactive (CLUTTER_ACTOR (item), TRUE);
}

HdLauncherItemType
hd_launcher_item_get_item_type (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), 0);

  return item->priv->item_type;
}

ClutterActor *
hd_launcher_item_get_icon (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), NULL);

  if (item->priv->icon)
    return item->priv->icon;

  return HD_LAUNCHER_ITEM_GET_CLASS (item)->get_icon (item);
}

ClutterActor *
hd_launcher_item_get_label (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), NULL);

  if (item->priv->label)
    return item->priv->label;

  return HD_LAUNCHER_ITEM_GET_CLASS (item)->get_label (item);
}

const gchar *
hd_launcher_item_get_text (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), NULL);

  if (item->priv->label)
    return clutter_label_get_text (CLUTTER_LABEL (item->priv->label));

  return NULL;
}
