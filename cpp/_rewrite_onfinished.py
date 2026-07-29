#!/usr/bin/env python3
"""Simplify onVersionDownloadFinished: replace Phase1/Phase2 merged logic with simple context loop."""

with open('src/backend/version_backend.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Phase 1 marker
phase1_marker = '    // ── Phase 1: process merged sessions whose MC version jus'
phase1_comment_end = '    // ── Phase 2: remaining merged sessions when all MC downlo'

# Find the start of Phase 1 comment block
phase1_start = content.index(phase1_marker)
# Find Phase 2 start (the end of Phase 1 block)
phase2_start = content.index(phase2_marker := '    // ── Phase 2: remaining merged sessions when all MC downlo')

# Find the end of Phase 2 block - it's the "Emit installComplete for non-merged" comment
emit_complete_marker = '    // Emit installComplete for non-merged installs ONLY.'
emit_complete_idx = content.index(emit_complete_marker)

# Also find the hasPendingLoader section at the bottom
next_queue_marker = '    // ── Try to start next from queue ──'
next_queue_idx = content.index(next_queue_marker)

# Find the start of hasPendingLoader check (after emit installComplete)
pending_loader_marker = '    // Check ALL sessions for pending loaders waiting for this MC version'
pending_start = content.index(pending_loader_marker)

# Find the end of hasPendingLoader block (before next queue)
has_pending_end = content.index('startNextFromQueue', pending_start)
# Go back to the beginning of the line containing startNextFromQueue
has_pending_end = content.rfind('\n', 0, has_pending_end) + 1

# Now replace the Phase1 + Phase2 block with a simple merged context loop
new_merged_block = '''    // ── Process merged sessions waiting for this MC version ──
    for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {
        auto* ctx = it.value();
        if (ctx->mcVersion == finishedId && !ctx->mcDownloadDone) {
            ctx->mcDownloadDone = true;
            isMergedInstall = true;
            qDebug() << "[install] Merged: MC complete for" << ctx->installId;

            // Update session steps
            auto* sessionDs = dlSession(ctx->installId);
            if (sessionDs) {
                sessionDs->mcBytesDl = sessionDs->mcBytesAll;
                for (int i = 0; i < 3 && i < sessionDs->steps.size(); i++)
                    updateStep(ctx->installId, i, QStringLiteral("completed"), 100);
                int verifyIdx = 3;
                if (verifyIdx < sessionDs->steps.size() && !sessionDs->optifineJarParallel)
                    updateStep(ctx->installId, verifyIdx, QStringLiteral("completed"), 100);
            }

            // Check if loader is ready
            if (ctx->loaderJarReady) {
                if (ctx->failed) {
                    qDebug() << "[install] MC done, loader previously failed — finalizing as vanilla" << ctx->installId;
                    finishInstall(ctx->installId);
                } else {
                    qDebug() << "[install] MC done, loader ready — proceeding to install";
                    proceedToLoaderInstall(ctx->installId);
                }
            }
        }
    }

'''

# Replace Phase 1 + Phase 2 block
old_phase_block = content[phase1_start:emit_complete_idx]
content = content.replace(old_phase_block, new_merged_block, 1)

# Find the updated positions after replacement
# hasPendingLoader section
updated_pending_start = content.index('    // Check ALL sessions for pending loaders waiting for this MC version')
updated_next_queue_start = content.index('    // ── Try to start next from queue ──')

# Replace the hasPendingLoader section with nothing
old_pending_block = content[updated_pending_start:updated_next_queue_start]
# Remove it (just the comment and the hasPendingLoader check, keep the startNextFromQueue)
content = content.replace(old_pending_block, '', 1)

# Also fix: the "Emit installComplete for non-merged" comment should use isMergedInstall
# Let me verify this still exists after our replacement
if 'if (success && !isMergedInstall) {' not in content:
    print("ERROR: isMergedInstall check removed unexpectedly!")
    exit(1)

with open('src/backend/version_backend.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("onVersionDownloadFinished simplified successfully")
