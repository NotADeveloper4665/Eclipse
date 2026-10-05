#pragma once

#include <vulkan/vulkan.h>

// Pick a present mode using the modes advertised for this Vulkan surface.
// Mailbox is opt-in with V-Sync; otherwise preserve Moonlight's existing
// immediate/relaxed/FIFO behavior.
inline VkPresentModeKHR chooseVulkanPresentMode(bool enableVsync,
                                                bool enableMailboxPresentMode,
                                                bool immediateSupported,
                                                bool relaxedSupported,
                                                bool mailboxSupported)
{
    if (enableVsync) {
        return enableMailboxPresentMode && mailboxSupported ?
                    VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
    }

    if (immediateSupported) return VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (relaxedSupported) return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (mailboxSupported) return VK_PRESENT_MODE_MAILBOX_KHR;
    return VK_PRESENT_MODE_FIFO_KHR;
}
