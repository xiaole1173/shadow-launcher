// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Scaffolding Room Code: U/NNNN-NNNN-SSSS-SSSS
#pragma once
#include <QString>
#include <optional>

namespace RoomCode {

struct Parts {
    QString networkName;  // scaffolding-mc-NNNN-NNNN
    QString networkKey;   // SSSS-SSSS
    QString displayCode;  // U/NNNN-NNNN-SSSS-SSSS
};

// Generate random room code with valid checksum
Parts generate();

// Parse and validate a room code string
// Returns nullopt if checksum fails or format is wrong
std::optional<Parts> parse(const QString& code);

// Extract network name/key without validation (for display)
bool isValidFormat(const QString& code);

} // namespace RoomCode
