/* nautilus-computer-view.h
 *
 * Native "This PC / My Computer" view for computer:///.
 * UI layout inspired by nautilus-my-computer (https://github.com/yannmasoch/nautilus-my-computer),
 * Copyright (c) 2026 Yann Masoch (MIT License).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "nautilus-window-slot.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_COMPUTER_VIEW (nautilus_computer_view_get_type ())
G_DECLARE_FINAL_TYPE (NautilusComputerView, nautilus_computer_view, NAUTILUS, COMPUTER_VIEW, AdwBin)

NautilusComputerView *nautilus_computer_view_new (NautilusWindowSlot *slot);

G_END_DECLS
