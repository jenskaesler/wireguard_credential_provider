#pragma once
//
// FieldDescriptors.h
//
// Central definition of all Credential Provider fields.
//

#include "helpers.h"

static const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR g_rgFields[] =
{
    { FI_TILEIMAGE, CPFT_TILE_IMAGE,   L"Icon"                      },
    { FI_LABEL,     CPFT_LARGE_TEXT,   L"WireGuard VPN"             },
    { FI_STATUS,    CPFT_SMALL_TEXT,   L"\u25CB Disconnected"       },
    { FI_TRAFFIC,   CPFT_SMALL_TEXT,   L""                          },
    { FI_PROFILE,   CPFT_COMBOBOX,     L"Profile"                   },
    { FI_SC_STATUS, CPFT_SMALL_TEXT,   L""                          },
    { FI_PIN,       CPFT_PASSWORD_TEXT,L"PIN"                       },
    { FI_BUTTON,    CPFT_COMMAND_LINK, L"Connect"                   },
};
static_assert(ARRAYSIZE(g_rgFields) == FI_NUM_FIELDS,
              "Field count does not match FI_NUM_FIELDS");

static const FIELD_STATE_PAIR g_rgFieldStates[] =
{
    { CPFS_DISPLAY_IN_BOTH,          CPFIS_NONE        }, // FI_TILEIMAGE
    { CPFS_DISPLAY_IN_BOTH,          CPFIS_NONE        }, // FI_LABEL
    { CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_NONE        }, // FI_STATUS
    { CPFS_HIDDEN,                   CPFIS_NONE        }, // FI_TRAFFIC
    { CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_INTERACTIVE }, // FI_PROFILE
    { CPFS_HIDDEN,                   CPFIS_NONE        }, // FI_SC_STATUS (visible when SC active)
    { CPFS_HIDDEN,                   CPFIS_INTERACTIVE }, // FI_PIN (visible when SC+PIN active)
    { CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_INTERACTIVE }, // FI_BUTTON
};
static_assert(ARRAYSIZE(g_rgFieldStates) == FI_NUM_FIELDS,
              "State count does not match FI_NUM_FIELDS");
