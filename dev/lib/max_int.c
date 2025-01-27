#include <stdio.h>
#include <stdarg.h>
#include <limits.h> // 用于INT_MIN

// 定义变参函数，参数n表示后续有多少个int类型的参数
int max_of_ints(int n, ...) {
    va_list args;
    va_start(args, n); // 初始化va_list变量，指向第一个变参

    int max = INT_MIN; // 初始化最大值为int类型的最小值

    for (int i = 0; i < n; i++) {
        int num = va_arg(args, int); // 获取下一个int类型的参数
        if (num > max) {
            max = num;
        }
    }

    va_end(args); // 结束变参的使用
    return max;
}

int main() {
    int a = 10, b = 20, c = 15, d = 30, e = 25, f = 40, g = 35, h = 50, i = 45, j = 60;
    int max = max_of_ints(10, a, b, c, d, e, f, g, h, i, j);

    printf("最大值是: %d\n", max);

    return 0;
}