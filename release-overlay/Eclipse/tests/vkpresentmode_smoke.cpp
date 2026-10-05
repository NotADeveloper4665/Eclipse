#include "../app/streaming/video/ffmpeg-renderers/plvkpresentmode.h"

#include <cstdlib>
#include <iostream>

static void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main()
{
    check(chooseVulkanPresentMode(true, true, true, true, true) == VK_PRESENT_MODE_MAILBOX_KHR,
          "Fast-Sync selects Mailbox when the Vulkan surface exposes it");
    check(chooseVulkanPresentMode(true, true, true, true, false) == VK_PRESENT_MODE_FIFO_KHR,
          "Fast-Sync safely falls back to FIFO when Mailbox is unavailable");
    check(chooseVulkanPresentMode(true, false, true, true, true) == VK_PRESENT_MODE_FIFO_KHR,
          "ordinary V-Sync continues to use FIFO by default");
    check(chooseVulkanPresentMode(false, true, true, true, true) == VK_PRESENT_MODE_IMMEDIATE_KHR,
          "V-Sync off preserves Immediate mode preference");
    check(chooseVulkanPresentMode(false, true, false, true, true) == VK_PRESENT_MODE_FIFO_RELAXED_KHR,
          "V-Sync off preserves FIFO Relaxed fallback preference");
    check(chooseVulkanPresentMode(false, true, false, false, true) == VK_PRESENT_MODE_MAILBOX_KHR,
          "V-Sync off preserves Mailbox fallback when other modes are unavailable");
    check(chooseVulkanPresentMode(false, true, false, false, false) == VK_PRESENT_MODE_FIFO_KHR,
          "unsupported non-FIFO modes fall back to FIFO");
    std::cout << "PASS: Vulkan present mode preference and fallbacks\n";
}
