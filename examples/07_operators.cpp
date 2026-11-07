#include <iostream>
using namespace std;

int calls = 0;

bool touch(bool value) {
    calls++;
    return value;
}

int main() {
    int a = 5, b = 3, c;
    cout << "precedence: " << a + b * 2 << " " << (a + b) * 2 << " " << a - b - 1 << endl;
    cout << "comparison: " << (a > b) << " " << (a == b) << " " << (a != b) << " " << (a <= b) << endl;
    cout << "bitwise: " << (a & b) << " " << (a | b) << " " << (a ^ b) << " " << ~a << " " << (a << 2) << " " << (a >> 1) << endl;
    cout << "logical: " << (a && b) << " " << (a && 0) << " " << (!a || b) << endl;

    bool r = touch(false) && touch(true);
    cout << "short-circuit &&: r = " << r << ", calls = " << calls << endl;
    r = touch(true) || touch(true);
    cout << "short-circuit ||: r = " << r << ", calls = " << calls << endl;

    int x = 10;
    int pre = ++x;
    int post = x++;
    cout << "++x -> " << pre << ", x++ -> " << post << ", x = " << x << endl;
    x -= 4;
    x *= 3;
    x /= 2;
    x %= 7;
    x <<= 3;
    x |= 1;
    x ^= 2;
    x &= 0xFF;
    cout << "compound: " << x << endl;

    int big = a > b ? (a > 10 ? 100 : 50) : 0;
    cout << "ternary: " << big << endl;

    int i, j;
    for (i = 0, j = 10; i < j; i += 3, j -= 3)
        cout << "(" << i << "," << j << ") ";
    cout << endl;

    char ch = 'A';
    ch = ch + 2;
    cout << "char: " << ch << " " << ch + 1 << " " << static_cast<char>('a' + 25) << " " << +ch << endl;
    cout << "literals: " << 0x1F << " " << 017 << " " << static_cast<int>('\n') << endl;

    bool flag = 7;
    cout << "bool: " << flag << " " << true + true << " " << boolalpha << (a < b) << " " << !flag << endl;

    int k = 0;
    do {
        k++;
        if (k == 2)
            continue;
        if (k > 4)
            break;
        cout << "k=" << k << " ";
    } while (true);
    cout << endl;

    int p, q, s;
    p = q = s = 7;
    cout << "assignment chain: " << p << " " << q << " " << s << endl;
    return 0;
}
