#include <iostream>
using namespace std;

const int N = 3;

int square(int v) {
    return v * v;
}

int maxOf(int a, int b) {
    return a > b ? a : b;
}

double maxOf(double a, double b) {
    return a > b ? a : b;
}

void increment(int& x) {
    x++;
}

void hello() {
    return 5;
}

int main() {
    int a = 1;
    int a = 2;
    N = 4;
    double d = 2.5;
    int arr[N] = {1, 2, 3, 4};
    undefinedVar = 3;
    square(1, 2);
    d % 2;
    increment(5);
    maxOf(1, 2.5);
    auto z;
    int text = "hello";
    std::cot << a;
    break;
    return missing();
}
