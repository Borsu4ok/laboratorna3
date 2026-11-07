#include <iostream>
#include <iomanip>
using namespace std;

const int LIMIT = 100;
bool composite[LIMIT + 1];

int main() {
    for (int i = 2; i * i <= LIMIT; i++) {
        if (composite[i])
            continue;
        for (int j = i * i; j <= LIMIT; j += i)
            composite[j] = true;
    }

    cout << "Primes up to " << LIMIT << ":" << endl;
    int count = 0;
    for (int i = 2; i <= LIMIT; i++) {
        if (!composite[i]) {
            cout << setw(4) << i;
            count++;
            if (count % 10 == 0)
                cout << endl;
        }
    }
    cout << endl << "Total: " << count << " primes" << endl;

    int firstBig = -1;
    for (int k = 90; k <= LIMIT; k++) {
        if (!composite[k]) {
            firstBig = k;
            break;
        }
    }
    cout << "First prime >= 90: " << firstBig << endl;
    cout << "composite[91] = " << composite[91] << endl;
    cout << boolalpha << "composite[97] = " << composite[97] << endl;
}
