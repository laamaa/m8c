// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include "ios_window.h"

#import <UIKit/UIKit.h>

void ios_sync_window_geometry(SDL_Window *window) {
  SDL_PropertiesID properties = SDL_GetWindowProperties(window);
  UIWindow *ui_window = (__bridge UIWindow *)SDL_GetPointerProperty(
      properties, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
  UIWindowScene *window_scene = ui_window.windowScene;
  if (window_scene == nil) {
    return;
  }

  CGRect frame;
  if (@available(iOS 26.0, *)) {
    frame = window_scene.effectiveGeometry.coordinateSpace.bounds;
  } else {
    frame = window_scene.coordinateSpace.bounds;
  }

  UIView *root_view = ui_window.rootViewController.view;
  if (CGRectEqualToRect(ui_window.frame, frame) &&
      CGRectEqualToRect(root_view.frame, ui_window.bounds)) {
    return;
  }

  ui_window.frame = frame;
  root_view.frame = ui_window.bounds;
  [ui_window setNeedsLayout];
  [ui_window layoutIfNeeded];
}
