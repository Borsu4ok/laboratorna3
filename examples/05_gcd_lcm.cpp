#include <iostream>
using namespace std;

int gcd(int a, int b) {
    while (b != 0) {
        int r = a % b;
        a = b;
        b = r;
    }
    return a;
}

int gcdRecursive(int a, int b) {
    if (b == 0)
        return a;
    else
        return gcdRecursive(b, a % b);
}

int lcm(int a, int b) {
    return a / gcd(a, b) * b;
}

int digitSum(int n) {
    int s = 0;
    do {
        s += n % 10;
        n /= 10;
    } while (n > 0);
    return s;
}

int main() {
    int pairs[] = {12, 18, 100, 75, 17, 5, 270, 192};
    for (int i = 0; i < 8; i += 2) {
        int a = pairs[i], b = pairs[i + 1];
        cout << "gcd(" << a << ", " << b << ") = " << gcd(a, b)
             << ", recursive = " << gcdRecursive(a, b)
             << ", lcm = " << lcm(a, b) << endl;
    }
    cout << "digitSum(987654321) = " << digitSum(987654321) << endl;
    cout << "digitSum(0) = " << digitSum(0) << endl;
    return 0;
}
