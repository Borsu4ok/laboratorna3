#include <iostream>
#include <iomanip>
#include <cmath>
using namespace std;

double newtonSqrt(double x) {
    double guess = x / 2.0;
    for (int i = 0; i < 20; i++)
        guess = (guess + x / guess) / 2;
    return guess;
}

double f(double x) {
    return x * x;
}

double integrate(double a, double b, int n) {
    double h = (b - a) / n;
    double s = (f(a) + f(b)) / 2;
    for (int i = 1; i < n; i++)
        s += f(a + i * h);
    return s * h;
}

int main() {
    cout << "7 / 2 = " << 7 / 2 << ", 7 / 2.0 = " << 7 / 2.0 << endl;
    cout << "static_cast<double>(7) / 2 = " << static_cast<double>(7) / 2 << endl;
    cout << "-7 / 2 = " << -7 / 2 << ", -7 % 2 = " << -7 % 2 << endl;
    cout << "(int)3.99 = " << (int)3.99 << ", static_cast<int>(-3.99) = " << static_cast<int>(-3.99) << endl;
    int truncated = 9.75;
    cout << "int truncated = 9.75 -> " << truncated << endl;

    cout << "default: " << 1.0 / 3 << " " << 0.1 + 0.2 << " " << 100000.0 << " " << 1234567.0 << " " << 0.00001 << endl;
    cout << "sqrt(2)  = " << sqrt(2) << endl;
    cout << "abs(-5) = " << abs(-5) << ", abs(-2.5) = " << abs(-2.5) << ", pow(2, 10) = " << pow(2, 10) << endl;

    cout << fixed << setprecision(10);
    cout << "newtonSqrt(2) = " << newtonSqrt(2) << endl;
    cout << "std::sqrt(2)  = " << std::sqrt(2.0) << endl;
    cout << setprecision(6) << "integral of x^2 on [0, 3] = " << integrate(0, 3, 1000) << endl;
    cout << setprecision(2) << "[" << setw(8) << 3.14159 << "] [" << left << setw(8) << 2.5 << "]" << right << endl;
    return 0;
}
