#ifndef __HD_SCROLLABLE_GROUP_H__
#define __HD_SCROLLABLE_GROUP_H__

#include <clutter/clutter.h>
#include <tidy/tidy-scroll-view.h>

#define HD_TYPE_SCROLLABLE_GROUP            (hd_scrollable_group_get_type ())
#define HD_SCROLLABLE_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_SCROLLABLE_GROUP, HdScrollableGroup))
#define HD_SCROLLABLE_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_SCROLLABLE_GROUP, HdScrollableGroupClass))
#define HD_IS_SCROLLABLE_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_SCROLLABLE_GROUP))
#define HD_IS_SCROLLABLE_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_SCROLLABLE_GROUP))
#define HD_SCROLLABLE_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_SCROLLABLE_GROUP, HdScrollableGroupClass))


typedef struct _HdScrollableGroup        HdScrollableGroup;
typedef struct _HdScrollableGroupClass   HdScrollableGroupClass;
typedef struct _HdScrollableGroupPrivate HdScrollableGroupPrivate;

typedef enum
{
  /* Start counting from 1 as these are used as
   * #GObject property IDs too. */
  HD_SCROLLABLE_GROUP_HORIZONTAL = 1,
  HD_SCROLLABLE_GROUP_VERTICAL,
} HdScrollableGroupDirection;

struct _HdScrollableGroup
{
  ClutterGroup	parent;

  HdScrollableGroupPrivate *priv; 
};

struct _HdScrollableGroupClass
{
  ClutterGroupClass	parent_class;

  
};

GType hd_scrollable_group_get_type (void);

ClutterActor *hd_scrollable_group_new (void);

gboolean hd_scrollable_group_is_clicked (HdScrollableGroup *self);

guint hd_scrollable_group_get_viewport_x (HdScrollableGroup *self);

guint hd_scrollable_group_get_viewport_y (HdScrollableGroup *self);

void hd_scrollable_group_set_viewport_x (HdScrollableGroup *self, guint x);

void hd_scrollable_group_set_viewport_y (HdScrollableGroup *self, guint y);

void hd_scrollable_group_set_real_estate (HdScrollableGroup *self,
                                          HdScrollableGroupDirection direction,
                                          guint cval);

/* Utility function that really belongs to #TidyScrollView. */
void tidy_scroll_view_show_scrollbar (TidyScrollView * self,
                                      HdScrollableGroupDirection which,
                                      gboolean enable);

#endif /* ! __HD_SCROLLABLE_GROUP_H__ */
