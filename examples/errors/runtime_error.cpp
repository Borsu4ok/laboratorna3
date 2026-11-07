#include <iostream>
using namespace std;

int divide(int a, int b) {
    return a / b;
}

int main() {
    int values[5] = {10, 20, 30, 40, 50};
    for (int i = 0; i <= 5; i++) {
        cout << "values[" << i << "] = " << values[i] << endl;
    }
    cout << divide(1, 0) << endl;
    return 0;
}
