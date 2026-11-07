#include <iostream>
#include <iomanip>
using namespace std;

long long factorial(int n) {
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}

int main() {
    for (int i = 0; i <= 12; i++) {
        cout << setw(2) << i << "! = " << factorial(i) << endl;
    }
    return 0;
}
