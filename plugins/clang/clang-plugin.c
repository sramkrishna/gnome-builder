/* clang-plugin.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <libpeas/peas.h>

#include "ide-clang-completion-item.h"
#include "ide-clang-completion-provider.h"
#include "ide-clang-diagnostic-provider.h"
#include "ide-clang-highlighter.h"
#include "ide-clang-private.h"
#include "ide-clang-service.h"
#include "ide-clang-symbol-node.h"
#include "ide-clang-symbol-resolver.h"
#include "ide-clang-symbol-tree.h"
#include "ide-clang-translation-unit.h"

void
peas_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_HIGHLIGHTER,
                                              IDE_TYPE_CLANG_HIGHLIGHTER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SYMBOL_RESOLVER,
                                              IDE_TYPE_CLANG_SYMBOL_RESOLVER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SERVICE,
                                              IDE_TYPE_CLANG_SERVICE);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                              IDE_TYPE_CLANG_DIAGNOSTIC_PROVIDER);
}
