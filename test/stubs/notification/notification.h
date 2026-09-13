#pragma once
typedef struct NotificationApp NotificationApp;
typedef struct { int dummy; } NotificationSequence;
void notification_message(NotificationApp* app, const NotificationSequence* seq);
