#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include <vulkan/vulkan.h>

#define CHECK(v) if (v != VK_SUCCESS) {fprintf(stderr, "%s failed with error %d\n", #v, v); exit(1); }

int main(int argc, char **argv) {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "Fyrebird",
        .applicationVersion = VK_MAKE_VERSION(0, 9, 42),

        .pEngineName = "C99",
        .engineVersion = VK_MAKE_VERSION(42, 9, 0),

        .apiVersion = VK_MAKE_VERSION(1, 0, 0),
    };
    VkInstanceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,

        .pApplicationInfo = &app_info,
    };

    VkInstance instance;
    CHECK(vkCreateInstance(&info, NULL, &instance));

    uint32_t pdev_count = 0;
    CHECK(vkEnumeratePhysicalDevices(instance, &pdev_count, NULL));

    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * pdev_count);
    CHECK(vkEnumeratePhysicalDevices(instance, &pdev_count, devices));

    for (uint32_t i = 0; i < pdev_count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        printf("device %d: %s\n", i, props.deviceName);
    }

    uint32_t pdev = 0;

    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[pdev], &props);
        printf("Using device %d: %s\n", pdev, props.deviceName);
    }

    VkDevice dev;

    uint32_t queue_family_index = 0;
    {
        {
            uint32_t count;

            vkGetPhysicalDeviceQueueFamilyProperties(devices[pdev], &count, NULL);

            VkQueueFamilyProperties *queue_families = malloc(sizeof(VkQueueFamilyProperties) * count);

            vkGetPhysicalDeviceQueueFamilyProperties(devices[pdev], &count, queue_families);

            for (uint32_t i = 0; i < count; i++) {
                if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT && queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
                    queue_family_index = i;
                    break;
                }
            }

            free(queue_families);
        }

        float half = 0.5;
        VkDeviceQueueCreateInfo queue_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,

            .queueFamilyIndex = queue_family_index,
            .queueCount = 1,
            .pQueuePriorities = &half,
        };

        VkDeviceCreateInfo dev_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = NULL,

            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queue_create_info,

            .enabledLayerCount = 0,
            .ppEnabledLayerNames = NULL,

            .enabledExtensionCount = 0,
            .ppEnabledExtensionNames = NULL,

            .pEnabledFeatures = NULL,
        };
        CHECK(vkCreateDevice(devices[pdev], &dev_info, NULL, &dev));
    }

    VkQueue queue;
    vkGetDeviceQueue(dev, queue_family_index, 0, &queue);

    VkCommandPool command_pool;

    {
        VkCommandPoolCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,

            .flags = 0,

            .queueFamilyIndex = queue_family_index,
        };

        CHECK(vkCreateCommandPool(
                dev,
                &create_info,
                NULL,
                &command_pool));

    }

    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,

        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = command_pool,
        .commandBufferCount = 1,
    };

    VkCommandBuffer buffer;
    CHECK(vkAllocateCommandBuffers(
            dev,
            &allocate_info,
            &buffer));

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,

        .flags = 0,

        .pInheritanceInfo = NULL,
    };

    /* create compute pipeline */
    VkShaderModule module;
    {
        size_t len = 0;
        size_t chunk_size = 256;
        unsigned char *code = NULL;
        FILE *fp = fopen("compute.spv", "rb");
        if (!fp) {
            perror("fopen");
        }
        while (true) {
            code = realloc(code, len + chunk_size);
            size_t amt = fread(&code[len], 1, chunk_size, fp);
            if (amt == 0) break;
            len += amt;
            chunk_size *= 2;
        }
        fclose(fp);

        VkShaderModuleCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .codeSize = len,
            .pCode = (uint32_t*) code,
        };

        CHECK(vkCreateShaderModule(dev,
            &create_info,
            NULL,
            &module));

        free(code);
    }
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout set_layout;
    {
        {
            VkDescriptorSetLayoutBinding bindings[] = {
                {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    .pImmutableSamplers = NULL,
                },
                {
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    .pImmutableSamplers = NULL,
                },
                {
                    .binding = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    .pImmutableSamplers = NULL,
                },
            };
            VkDescriptorSetLayoutCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .bindingCount = 3,
                .pBindings = &bindings[0],
            };
            CHECK(vkCreateDescriptorSetLayout(
                    dev,
                    &create_info,
                    NULL,
                    &set_layout));
        }

        VkPipelineLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,

            .setLayoutCount = 1,
            .pSetLayouts = &set_layout,

            .pushConstantRangeCount = 0,
            .pPushConstantRanges = NULL,
        };
        CHECK(vkCreatePipelineLayout(
                dev,
                &create_info,
                NULL,
                &pipeline_layout
            ));
    }
    VkPipeline pipeline;
    {
        VkComputePipelineCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = NULL,

            .flags = 0,

            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .module = module,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .pName = "main",
                .pSpecializationInfo = NULL,
            },
            .layout = pipeline_layout,
        };
        CHECK(vkCreateComputePipelines(
                dev,
                NULL,
                1,
                &create_info,
                NULL,
                &pipeline));
    }
    VkDescriptorPool descriptor_pool;
    {
        VkDescriptorPoolCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = NULL,

            .flags = 0,

            .maxSets = 1,
            .poolSizeCount = 2,
            .pPoolSizes = (VkDescriptorPoolSize[]) {
                {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 64,
                },
                {
                    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 64,
                },
            },
        };
        CHECK(vkCreateDescriptorPool(
                dev,
                &create_info,
                NULL,
                &descriptor_pool
            ));
    }

    VkDescriptorSet descriptor_set;
    {
        VkDescriptorSetAllocateInfo info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = NULL,

            .descriptorPool = descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &set_layout,
        };
        CHECK(vkAllocateDescriptorSets(dev, &info, &descriptor_set));
    }

    VkDeviceMemory memory;
    {
        uint32_t index = 0xFFFFFFFF;
        VkPhysicalDeviceMemoryProperties props;
        vkGetPhysicalDeviceMemoryProperties(devices[pdev], &props);
        for (size_t i = 0; i < props.memoryTypeCount; i++) {
            if (
                props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT &&
                props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            ) {
                index = i;
                break;
            }
        }
        if (index == 0xFFFFFFFF) {
            printf("no valid memory types!\n");
        }
        VkMemoryAllocateInfo info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = NULL,
            .allocationSize = 256 * 1024 * 1024,
            .memoryTypeIndex = index,
        };
        CHECK(vkAllocateMemory(
            dev,
            &info,
            NULL,
            &memory));
    }

    VkBuffer buf;
    {

        VkBufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = NULL,

            .flags = 0,

            .size = 256 * 1024 * 1024,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,

            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,

            .queueFamilyIndexCount = 0,
        };
        CHECK(vkCreateBuffer(dev, &create_info, NULL, &buf));

        vkBindBufferMemory(dev, buf, memory, 0);
    }

    {
        VkWriteDescriptorSet writes[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = descriptor_set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,

                .pImageInfo = NULL,
                .pBufferInfo = (VkDescriptorBufferInfo[]) {
                    {
                        .buffer = buf,
                        .offset = 0,
                        .range = 4,
                    },
                },
                .pTexelBufferView = NULL,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = descriptor_set,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,

                .pImageInfo = NULL,
                .pBufferInfo = (VkDescriptorBufferInfo[]) {
                    {
                        .buffer = buf,
                        .offset = 64,
                        .range = 64,
                    },
                },
                .pTexelBufferView = NULL,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = descriptor_set,
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,

                .pImageInfo = NULL,
                .pBufferInfo = (VkDescriptorBufferInfo[]) {
                    {
                        .buffer = buf,
                        .offset = 128,
                        .range = 1920 * 1080 * 4 * 4,
                    },
                },
                .pTexelBufferView = NULL,
            },
        };
        vkUpdateDescriptorSets(dev,
            3,
            &writes[0],
            0,
            NULL);
    }

    CHECK(vkBeginCommandBuffer(buffer, &begin_info));
    uint32_t in_data[] = {
        1920,
        1080,
    };
    vkCmdUpdateBuffer(buffer, buf, 0, sizeof(in_data), &in_data[0]);
    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(
        buffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline_layout,
        0,
        1,
        &descriptor_set,
        0,
        NULL);
    vkCmdDispatch(buffer, 1920, 1080, 1);
    CHECK(vkEndCommandBuffer(buffer));

    // submit
    VkSubmitInfo submits[] = {
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 0,
            .commandBufferCount = 1,
            .pCommandBuffers = &buffer,
            .signalSemaphoreCount = 0,
        },
    };
    CHECK(vkQueueSubmit(
            queue,
            1,
            submits,
            VK_NULL_HANDLE));

    CHECK(vkQueueWaitIdle(queue));

    {
        void *data;
        CHECK(vkMapMemory(
                dev,
                memory,
                128,
                128 + (1920 * 1080 * 4 * 4),
                0,
                &data));

        printf("writing hdr...\n");
        stbi_write_hdr("out.hdr", 1920, 1080, 4, data);
    };

    vkFreeMemory(dev, memory, NULL);
    vkDestroyBuffer(dev, buf, NULL);
    vkDestroyDescriptorPool(dev, descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(dev, set_layout, NULL);
    vkDestroyShaderModule(dev, module, NULL);
    vkDestroyPipelineLayout(dev, pipeline_layout, NULL);
    vkDestroyPipeline(dev, pipeline, NULL);
    vkFreeCommandBuffers(dev, command_pool, 1, &buffer);

    vkDestroyCommandPool(dev, command_pool, NULL);
    vkDestroyDevice(dev, NULL);
    vkDestroyInstance(instance, NULL);
}
