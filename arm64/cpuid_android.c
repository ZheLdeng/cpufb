#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<stdbool.h>

#define MAX_FEATURES 100
#define MAX_FEATURE_LENGTH 1000

bool find(const char *str, char (*features)[MAX_FEATURE_LENGTH], int count) {
    for (int i = 0; i < count; i++) {
        if (strcmp(str, features[i]) == 0) {
            return true;
        }
    }
    return false;
}

int getCPUFeatures(char (*features)[MAX_FEATURE_LENGTH], int max_features) {
    char command[] = "cat /proc/cpuinfo | grep Features | head -n1";
    FILE* pipe = popen(command, "r");
    int count = 0;

    if (!pipe) {
        perror("popen() failed");
        return -1;
    }

    // 读取命令输出并存储到数组中
    while (fgets(features[count], MAX_FEATURE_LENGTH, pipe)) {
        // 移除换行符
        features[count][strlen(features[count]) - 1] = '\0';

        // 使用冒号进行分割
        char *feature = strtok(features[count], ":");
        feature = strtok(NULL, ":"); // 忽略冒号前的部分

        // 使用空格进行分割
        char *token = strtok(feature, " ");
        while (token != NULL) {
            // 存储特性项到数组中
            strcpy(features[count], token);
            count++;
            if (count >= max_features) {
                fprintf(stderr, "Too many features\n");
                break;
            }
            token = strtok(NULL, " ");
        }
    }

    pclose(pipe);
    return count;
}

int main() {
    char features[MAX_FEATURES][MAX_FEATURE_LENGTH];

    int count = getCPUFeatures(features, MAX_FEATURES);
    // for (int i = 0; i < count; i++) {
    //     printf("%s\n", features[i]);
    // }
    if (find("asimd",features,count)) {
        printf("_ASIMD_\n");
    }
    if (find("asimddp",features,count)) {
        printf("_ASIMD_DP_\n");
    }
    if (find("asimdhp",features,count)) {
        printf("_ASIMD_HP_\n");
    }
    if (find("i8mm",features,count)) {
        printf("_I8MM_\n");
    }
    if (find("bf16",features,count)) {
        printf("_BF16_\n");
    }
    if (find("sme",features,count)) {
        printf("_SME_\n");
    }
    if (find("sve",features,count)) {
        printf("_SVE_\n");
    }
    if (find("sme2",features,count)) {
        printf("_SME2_\n");
        if (find("sme_f64f64",features,count)) {
            printf("_SMEf64_\n");
        }
    }
    printf("_LDP_\n");     
    printf("_ISSUE_\n");    
    return 0;
}
