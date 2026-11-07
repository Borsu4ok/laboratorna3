#include <iostream>
using namespace std;

long long fibIterative(int n) {
    long long a = 0, b = 1;
    int i = 0;
    while (i < n) {
        long long next = a + b;
        a = b;
        b = next;
        i++;
    }
    return a;
}

int fibRecursive(int n) {
    return n < 2 ? n : fibRecursive(n - 1) + fibRecursive(n - 2);
}

int main() {
    for (int n = 0; n <= 15; n++) {
        cout << "fib(" << n << ") = " << fibRecursive(n) << endl;
    }
    cout << "fib(50) = " << fibIterative(50) << endl;
    return 0;
}
