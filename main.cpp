#include <iostream>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <cstring>
using namespace std;

// Convert a 32-bit float to 16-bit half precision (with rounding)
uint16_t floatToHalf(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits)); // Reinterpret float bits

    uint32_t sign = (bits >> 31) & 0x1;
    int32_t exp = (bits >> 23) & 0xFF;
    uint32_t mant = bits & 0x7FFFFF; // 23 bits

    uint16_t hSign, hExp, hMant;

    // Handle NaN or Inf
    if (exp == 255) {
        hExp = 0x1F;
        hMant = (mant ? 0x200 : 0); // NaN or Inf
    }
    else if (exp > 142) { // Overflow → Inf
        hExp = 0x1F;
        hMant = 0;
    }
    else if (exp < 113) { // Underflow → Subnormal or zero
        if (exp < 103) { // Too small → zero
            hExp = 0;
            hMant = 0;
        } else {
            int shift = 113 - exp; // how many bits to denormalize
            uint32_t mant_with_hidden = 0x800000 | mant;
            uint32_t shifted = mant_with_hidden >> (shift - 1);

            // Rounding (nearest even)
            uint32_t round_bit = shifted & 0x1;
            shifted >>= 1;
            if (round_bit && (shifted & 1 || (mant_with_hidden & ((1 << (shift - 1)) - 1))))
                shifted++;

            hExp = 0;
            hMant = shifted >> 13;
        }
    }
    else { // Normalized
        hExp = exp - 112;
        uint32_t mant_to_round = mant;

        // Extract rounding bits (guard, round, sticky)
        uint32_t guard = (mant_to_round >> 12) & 1;
        uint32_t round_bit = (mant_to_round >> 11) & 1;
        uint32_t sticky = mant_to_round & 0x7FF; // lower 11 bits

        // Shift mantissa down to 10 bits
        hMant = mant_to_round >> 13;

        // Round to nearest even
        if (guard && (round_bit || sticky || (hMant & 1))) {
            hMant++;
            if (hMant == 0x400) { // mantissa overflow (1.111... → 10.000)
                hMant = 0;
                hExp++;
                if (hExp >= 0x1F) { // exponent overflow → Inf
                    hExp = 0x1F;
                    hMant = 0;
                }
            }
        }
    }

    hSign = sign << 15;
    return (uint16_t)(hSign | (hExp << 10) | hMant);
}

// Convert 16-bit half to 32-bit float (for verification)
float halfToFloat(uint16_t h) {
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exp = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    uint32_t bits;

    if (exp == 0) {
        if (mant == 0) {
            bits = sign << 31; // Zero
        } else { // Subnormal
            exp = 1;
            while ((mant & 0x400) == 0) {
                mant <<= 1;
                exp--;
            }
            mant &= 0x3FF;
            exp = exp - 1 + 127 - 15;
            bits = (sign << 31) | (exp << 23) | (mant << 13);
        }
    } else if (exp == 0x1F) { // Inf/NaN
        bits = (sign << 31) | (0xFF << 23) | (mant << 13);
    } else { // Normalized
        exp = exp - 15 + 127;
        bits = (sign << 31) | (exp << 23) | (mant << 13);
    }

    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

// Pretty-print 16-bit binary form
void printBinary16(uint16_t val) {
    for (int i = 15; i >= 0; --i) {
        cout << ((val >> i) & 1);
        if (i == 15 || i == 10) cout << ' ';
    }
}

class splitNumber {
    public:
    bool sign;
    int exponent;
    int unbiasedExponent;
    uint32_t mantissa;

    void assignUint16_t(uint16_t number) {
        sign = number >> 15;
        // cout << "sign: " << (number >> 15) << endl;
        exponent = (number >> 10) & 0b0000000000011111;
        unbiasedExponent = exponent - 15;
        mantissa = number & 0b0000001111111111;
        if (exponent != 0) {
            mantissa |= 0b10000000000; // add the implicit leading one for normalized numbers
        }
    }
};

void printMantissa(uint32_t mantissa) {
    for (int i = 23; i >= 0; i--) {
        cout << ((mantissa >> i) & 1);
    }
}

uint16_t divideNumbers2(uint16_t Dividend, uint16_t Divisor) {
    splitNumber a, b, q;
    a.assignUint16_t(Dividend);
    b.assignUint16_t(Divisor);

    q.sign = a.sign ^ b.sign;
    q.unbiasedExponent = (a.unbiasedExponent - b.unbiasedExponent);
    uint32_t mantA = a.mantissa;
    uint32_t mantB = b.mantissa;

    // Perform high-precision division
    uint64_t numerator = (uint64_t)mantA << 13; // extra bits
    uint32_t raw = numerator / mantB;

    // Normalize
    while (raw && raw < (1 << 23)) { raw <<= 1; q.unbiasedExponent--; }
    while (raw >= (2 << 23)) { raw >>= 1; q.unbiasedExponent++; }

    // Round to nearest even
    uint32_t rounding = (raw >> 13) & 1;
    if (rounding && (raw & 0x1FFF)) raw += (1 << 13);

    q.mantissa = (raw >> 13) & 0x3FF;
    q.exponent = q.unbiasedExponent + 15;

    if (q.exponent <= 0) { q.exponent = 0; q.mantissa = 0; } // underflow
    if (q.exponent >= 31) { q.exponent = 31; q.mantissa = 0; } // overflow → inf

    return (q.sign << 15) | (q.exponent << 10) | q.mantissa;
}

uint16_t divideNumbers(uint16_t Dividend, uint16_t Divisor, bool debug = false, int* quotient2 = nullptr, bool* expadded = nullptr) {

    uint16_t result = 0b0111111000000000;

    splitNumber dividend,divisor,quotient;
    dividend.assignUint16_t(Dividend);
    divisor.assignUint16_t(Divisor);
    quotient.assignUint16_t(result);

    quotient.sign = dividend.sign ^ divisor.sign;

    quotient.unbiasedExponent = dividend.unbiasedExponent - divisor.unbiasedExponent;
    quotient.exponent = dividend.exponent - divisor.exponent + 15;
    //cout << "Quotient exponent (before normalisation): " << quotient.exponent << endl;

    uint32_t mantA = dividend.mantissa << 13; // so top of 24 bits
    uint32_t mantB = divisor.mantissa;

    if (mantB == 0) {
      return 0;
    }
    uint32_t quotient_mant = mantA / mantB; // integer division
    if (debug) {
        cout << "Raw quotient mantissa: ";
        printMantissa(quotient_mant);
        cout << endl;
    }
    

    // Normalize 
    bool subnormal = false;
    int dist_from_zero = 0;
    if (quotient.exponent <= 0) { 
        dist_from_zero = 1 - quotient.exponent; 
        quotient.exponent = 0;
        subnormal = true;
        //cout << "quotient less than 0\n";
    }
    if (debug) {
        cout << "exponent: " << quotient.exponent << endl;
        cout << "subnormal: " << subnormal << endl;
        cout << "Distance from zero: " << dist_from_zero << endl;
    }
    int shift_count = 0;
    while (quotient_mant && quotient_mant < (1 << (23 - (dist_from_zero << 1))) ) { // << 1 = * 2
        quotient_mant <<= 1; 
        if (quotient.exponent > -dist_from_zero) {
            quotient.exponent--;
        } else {
            if (debug) {
                cout << "Reached zero exponent during normalization.\n";
            }
            break;
        }
        shift_count++; 
        if (debug) {
            cout << "Shifted left once to normalize: ";
            printMantissa(quotient_mant);
            cout << endl;
        }
    }
    if (debug) {
        cout << shift_count << " shifts during normalization.\n";
    }

    quotient.exponent -= dist_from_zero; // adjust for denormals

    // cout << "Normalized quotient mantissa: ";
    // printMantissa(quotient_mant);
    // cout << endl;
    // cout << "Shifted left " << shift_count << " times during normalization.\n";

    //rounding
    // 0000 0000 00 00 0000 0000 0000
    if ((quotient_mant & 0b1000000000000000) && ((quotient_mant & 0b10000000000000000) | (quotient_mant & 0b100000000000000) | (quotient_mant & 0b11111111111111))) {
        quotient_mant += 1;
        if (quotient_mant & 0b1000000000000000000000000) {
            if (expadded) *expadded = true;
            quotient.exponent++;
        }
    }
    quotient.mantissa = (quotient_mant >> (13)) & 0b0000001111111111;

    if (quotient.exponent > 30) {
      quotient.exponent = 0b11111;
      quotient.mantissa = 0;
      return ((quotient.sign << 15) & 0b1000000000000000) | (((quotient.exponent + 10) << 10) & 0b0111110000000000) | (quotient.mantissa & 0b0000001111111111);
    }

    result = ((quotient.sign << 15) & 0b1000000000000000) | (((quotient.exponent + 10) << 10) & 0b0111110000000000) | (quotient.mantissa & 0b0000001111111111);



    if (debug) {
        cout << endl;
    }
    return result;
}

bool validate_division(float dividend, float divisor) {
    uint16_t halfA = floatToHalf(dividend);
    uint16_t halfB = floatToHalf(divisor);
    uint16_t halfResult = divideNumbers(halfA, halfB);
    uint16_t correctHalf = floatToHalf(dividend / divisor);
    float result = halfToFloat(halfResult);
    float correct = halfToFloat(correctHalf);
    return fabs(result - correct) < 1e-2; // tolerance for floating-point comparison
}

float random_half_value(bool includeSubnormals = true) {
    float val = 0.0f;

    if (includeSubnormals) {
        // --- Full range including subnormals ---
        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

        float min_val = 5.96046448e-8f; // smallest positive half
        float max_val = 65504.0f;       // largest normal half

        float log_min = logf(min_val);
        float log_max = logf(max_val);

        val = expf(log_min + r * (log_max - log_min));
    } else {
        // --- Normal-only range (skip subnormals) ---
        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

        float min_normal = 6.10352e-5f; // smallest positive normal half
        float max_normal = 65504.0f;    // largest normal half

        // Linear interpolation in normal range
        val = min_normal + r * (max_normal - min_normal);
    }

    // Random sign
    if (rand() & 1) val = -val;

    return val;
}


int main() {
    cout << "Select mode:\n";
    cout << "1. Manual test (enter two numbers)\n";
    cout << "2. Run many random tests\n> ";

    int mode;
    cin >> mode;

    if (mode == 1) {
        float a, b;
        cout << "Enter Dividend: ";
        cin >> a;
        cout << "Enter Divisor: ";
        cin >> b;
        if (b == 0.0f) { cout << "Division by zero!\n"; return 1; }

        uint16_t ha = floatToHalf(a);
        uint16_t hb = floatToHalf(b);
        uint16_t hr = divideNumbers(ha, hb, true);
        float result = halfToFloat(hr);
        cout << fixed << setprecision(8);
        printBinary16(ha);
        cout << " / ";
        printBinary16(hb);
        cout << "\n = ";
        printBinary16(hr);
        cout << "\n : ";
        printBinary16(floatToHalf(a/b));
        cout << "\nResult (FP16 division): " << result << endl;
        cout << "Reference (FP32 division): " << (a / b) << endl;
        cout << "Error: " << fabs(result - (a / b)) << endl;
    } 
    else if (mode == 2) {
        bool includeSubnormals = true;
        cout << "Include subnormals in test values? (1=yes,0=no): ";
        int subnormalsInput;
        cin >> subnormalsInput;
        includeSubnormals = (subnormalsInput != 0);

        int test_cases = 10000;
        int passed = 0;
        srand(42); // fixed seed for reproducibility
        
        for (int i = 0; i < test_cases; ++i) {
            float a = random_half_value(includeSubnormals);
            float b = random_half_value(includeSubnormals);
            if (b == 0.0f) b = 1.0f;
            if (a > 65504.0f) a = 65504.0f;

            if (validate_division(a, b)) passed++;
            else {
              if (i < 100) {
                cout << "Test failed for " << a << " / " << b << endl;
                uint16_t ha = floatToHalf(a);
                uint16_t hb = floatToHalf(b);
                bool expadded = false;
                uint16_t hr = divideNumbers(ha, hb, true, nullptr, &expadded);
                uint16_t correctHalf = floatToHalf(a / b);
                cout << "Computed half: ";
                printBinary16(hr);
                cout << " (float: " << halfToFloat(hr) << ")\n";
                cout << "Correct half:  ";
                printBinary16(correctHalf);
                cout << " (float: " << halfToFloat(correctHalf) << ")\n";
                cout << "Exponent was incremented due to rounding overflow: " << (expadded ? "yes" : "no") << endl;
              }
                
            }
        }
        cout << "Passed " << passed << " out of " << test_cases << " tests." << "[" << (passed * 100.0 / test_cases) << "%]" << endl;

    }   
    else {
        cout << "Invalid mode.\n";
    }

    // cout << "any key to exit...";
    // int the;
    // cin >> the;

    return 0;



    // cout << (0b1000000000000010 >> 15) << std::endl; 
    // float numberA = 1;
    // //cout << "Enter a floating-point number: ";
    // //cin >> numberA;

    // float numberB = 18452;
    // //cout << "Enter a floating-point number: ";
    // //cin >> numberB;

    // uint16_t halfA, halfB;
    // halfA = floatToHalf(numberA);
    // halfB = floatToHalf(numberB);
    // printBinary16(halfA);
    // cout << " \n";
    // printBinary16(halfB);
    // cout << "\n";

    // uint16_t halfResult = divideNumbers(halfA, halfB);
    // float back = halfToFloat(halfResult);

    // cout << fixed << setprecision(6);
    // cout << "Result 32b converted: " << back << endl;
    // cout << "Result 16b: ";
    // printBinary16(halfResult);
    // cout << "\n";

    // cout << "correct result: " << numberA / numberB << endl;
    // cout << "correct binary: ";
    // uint16_t correctHalf = floatToHalf(numberA / numberB);
    // printBinary16(correctHalf);
    // cout << "\n";
    // splitNumber check, mine;
    // check.assignUint16_t(correctHalf);
    // mine.assignUint16_t(halfResult);
    // cout << "exponent diff: " << - static_cast<int>(check.exponent) + static_cast<int>(mine.exponent) << endl;

    // correctHalf == halfResult ? cout << "Results match!" << endl : cout << "Results do not match!" << endl;




    // Extensive validation
    // auto res = divideNumbers2(halfA, halfB);
    // float back2 = halfToFloat(res);
    // cout << "Result 32b converted (method 2): " << back2 << endl;
    // cout << "Result 16b (method 2): ";
    // printBinary16(res);
    // cout << "\n";
    // cout << "\nOriginal: " << input << endl;
    // cout << "Half bits: ";
    // printBinary16(half);
    // cout << "\nHex: 0x" << hex << setw(4) << setfill('0') << half << dec << endl;
    // cout << "Decoded back: " << back << endl;
    //-0.00574281 / -294.676
}
