#ifndef __HD_SCROLLABLE_GROUP_H__
#define __HD_SCROLLABLE_GROUP_H__

#include <clutter/clutter.h>
#include <tidy/tidy-scroll-view.h>

#define HD_TYPE_SCROLLABLE_GROUP  hd_scrollable_group_get_type()
#define HD_SCROLLABLE_GROUP(obj)                        \
  G_TYPE_CHECK_INSTANCE_CAST((obj),                     \
                             HD_TYPE_SCROLLABLE_GROUP,  \
                             HdScrollableGroup)

typedef ClutterGroupClass HdScrollableGroupClass;
typedef ClutterGroup HdScrollableGroup;

typedef enum
{
  /* Start counting from 1 as these are used as
   * #GObject property IDs too. */
  HD_SCROLLABLE_GROUP_HORIZONTAL = 1,
  HD_SCROLLABLE_GROUP_VERTICAL,
} HdScrollableGroupDirection;

GType hd_scrollable_group_get_type (void);
ClutterActor *hd_scrollable_group_new (void);

gboolean hd_scrollable_group_is_clicked (HdScrollableGroup * self);

guint hd_scrollable_group_get_viewport_x (HdScrollableGroup * self);
guint hd_scrollable_group_get_viewport_y (HdScrollableGroup * self);
void hd_scrollable_group_set_viewport_x (HdScrollableGroup * self, guint x);
void hd_scrollable_group_set_viewport_y (HdScrollableGroup * self, guint y);

void hd_scrollable_group_scroll_viewport (HdScrollableGroup * self,
                                          HdScrollableGroupDirection which,
                                          gboolean is_relative, gint diff,
                                          GCallback fun,
                                          gpointer funparam);
void hd_scrollable_group_set_real_estate (HdScrollableGroup * self,
                                          HdScrollableGroupDirection which,
                                          guint cval);

/* Utility function that really belongs to #TidyScrollView. */
void tidy_scroll_view_show_scrollbar (TidyScrollView * self,
                                      HdScrollableGroupDirection which,
                                      gboolean enable);

#endif /* ! __HD_SCROLLABLE_GROUP_H__ */
