/* 
 * Copyright (C) 2010-2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef EEKBOARD_SERVICE_H
#define EEKBOARD_SERVICE_H 1

#define __EEKBOARD_SERVICE_H_INSIDE__ 1

#include "server-context-service.h"

#include "sm.puri.OSK0.h"

G_BEGIN_DECLS

#define EEKBOARD_SERVICE_PATH "/sm/puri/OSK0"
#define EEKBOARD_SERVICE_INTERFACE "sm.puri.OSK0"

/**
 * EekboardService: DBus handling
 */
typedef struct _EekboardService
{
    GDBusConnection *connection;
    SmPuriOSK0 *dbus_interface;
    GDBusNodeInfo *introspection_data;
    guint registration_id;
    char *object_path;

    ServerContextService *context; // unowned reference
} EekboardService;

EekboardService * eekboard_service_new      (GDBusConnection *connection,
                                             const gchar     *object_path);
void              eekboard_service_set_context(EekboardService *service,
                                               ServerContextService *context);
void eekboard_service_destroy(EekboardService*);
G_END_DECLS
#endif  /* EEKBOARD_SERVICE_H */
