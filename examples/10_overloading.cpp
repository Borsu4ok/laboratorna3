#include <iostream>
#include <cmath>
using namespace std;

int maxOf(int a, int b) {
    return a > b ? a : b;
}

double maxOf(double a, double b) {
    return a > b ? a : b;
}

int maxOf(int a, int b, int c) {
    return maxOf(maxOf(a, b), c);
}

void describe(int value) {
    cout << "int: " << value << endl;
}

void describe(double value) {
    cout << "double: " << value << endl;
}

void describe(char value) {
    cout << "char: '" << value << "'" << endl;
}

void describe(bool value) {
    cout << "bool: " << boolalpha << value << endl;
}

void minMax(const int a[], int n, int& lo, int& hi) {
    lo = a[0];
    hi = a[0];
    for (int i = 1; i < n; i++) {
        if (a[i] < lo)
            lo = a[i];
        if (a[i] > hi)
            hi = a[i];
    }
}

void increment(int& counter, int step) {
    counter += step;
}

int main() {
    cout << "maxOf(3, 8) = " << maxOf(3, 8) << endl;
    cout << "maxOf(2.5, 1.5) = " << maxOf(2.5, 1.5) << endl;
    cout << "maxOf(4, 9, 6) = " << maxOf(4, 9, 6) << endl;
    cout << "maxOf('a', 'z') = " << maxOf('a', 'z') << endl;

    describe(7);
    describe(3.5);
    describe('A');
    describe(1 < 2);

    int values[] = {4, -2, 17, 9, 0, 11};
    int lo, hi;
    minMax(values, 6, lo, hi);
    cout << "min = " << lo << ", max = " << hi << endl;

    int counter = 0;
    for (int v : values)
        increment(counter, v);
    cout << "counter = " << counter << endl;

    auto half = counter / 2.0;
    cout << "half = " << half << ", abs(-7) = " << abs(-7) << ", abs(-0.5) = " << abs(-0.5) << endl;
    return 0;
}
