#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "NumberObject.h"

namespace Escargot {

static int itoa(int64_t value, char* sp, int radix)
{
    char tmp[256]; // be careful with the length of the buffer
    char* tp = tmp;
    int i;
    uint64_t v;

    int sign = (radix == 10 && value < 0);
    if (sign)
        v = -value;
    else
        v = (uint64_t)value;

    while (v || tp == tmp) {
        i = v % radix;
        v /= radix; // v/=radix uses less CPU clocks than v=v/radix does
        if (i < 10)
            *tp++ = i + '0';
        else
            *tp++ = i + 'a' - 10;
    }

    int64_t len = tp - tmp;

    if (sign) {
        *sp++ = '-';
        len++;
    }

    while (tp > tmp) {
        *sp++ = *--tp;
    }
    *sp++ = 0;

    return len;
}

static Value builtinNumberConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    NumberObject* numObj;
    if (isNewExpression) {
        numObj = thisValue.asPointerValue()->asObject()->asNumberObject();
        if (argv[0].isUndefined())
            numObj->setPrimitiveValue(state, 0);
        else
            numObj->setPrimitiveValue(state, argv[0].toNumber(state));
        return numObj;
    } else {
        if (argc == 0)
            return Value(0);
        else
            return Value(argv[0].toNumber(state));
    }
}

static Value builtinNumberToFixed(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double number;

    if (thisValue.isNumber()) {
        number = thisValue.asNumber();
    } else if (thisValue.isPointerValue() && thisValue.asPointerValue()->isNumberObject()) {
        number = thisValue.asPointerValue()->asNumberObject()->primitiveValue();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toFixed.string(), errorMessage_GlobalObject_ThisNotNumber);
    }

    if (argc == 0) {
        bool isInteger = (static_cast<int64_t>(number) == number);
        if (isInteger) {
            char buffer[256];
            itoa(static_cast<int64_t>(number), buffer, 10);
            return new ASCIIString(buffer);
        } else {
            return Value(round(number)).toString(state);
        }
    } else if (argc >= 1) {
        double digitD = argv[0].toNumber(state);
        if (digitD == 0 || std::isnan(digitD)) {
            return Value(round(number)).toString(state);
        }
        int digit = (int)trunc(digitD);
        if (digit < 0 || digit > 20) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toFixed.string(), errorMessage_GlobalObject_RangeError);
        }
        if (std::isnan(number) || std::isinf(number)) {
            return Value(number).toString(state);
        } else if (std::abs(number) >= pow(10, 21)) {
            return Value(round(number)).toString(state);
        }

        std::basic_ostringstream<char> stream;
        if (number < 0)
            stream << "-";
        stream << "%." << digit << "lf";
        std::string fstr = stream.str();
        char buf[512];
        snprintf(buf, sizeof(buf), fstr.c_str(), std::abs(number));
        return Value(new ASCIIString(buf));
    }
    return Value();
}

static Value builtinNumberToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double number = 0.0;

    if (thisValue.isNumber()) {
        number = thisValue.asNumber();
    } else if (thisValue.isPointerValue() && thisValue.asPointerValue()->isNumberObject()) {
        number = thisValue.asPointerValue()->asNumberObject()->primitiveValue();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotNumber);
    }

    if (std::isnan(number) || std::isinf(number)) {
        return (Value(number).toString(state));
    }
    double radix = 10;
    if (argc > 0 && !argv[0].isUndefined()) {
        radix = argv[0].toInteger(state);
        if (radix < 2 || radix > 36)
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_RadixInvalidRange);
    }
    if (radix == 10)
        return (Value(number).toString(state));
    else {
        bool isInteger = (static_cast<int64_t>(number) == number);
        if (isInteger) {
            bool minusFlag = (number < 0) ? 1 : 0;
            number = (number < 0) ? (-1 * number) : number;
            char buffer[256];
            if (minusFlag) {
                buffer[0] = '-';
                itoa(static_cast<int64_t>(number), &buffer[1], radix);
            } else {
                itoa(static_cast<int64_t>(number), buffer, radix);
            }
            return (new ASCIIString(buffer));
        } else {
            ASSERT(Value(number).isDouble());
            NumberObject::RadixBuffer s;
            const char* str = NumberObject::toStringWithRadix(state, s, number, radix);
            return new ASCIIString(str);
        }
    }
    return Value();
}

void GlobalObject::installNumber(ExecutionState& state)
{
    m_number = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Number, builtinNumberConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                      return new NumberObject(state);
                                  }),
                                  FunctionObject::__ForBuiltin__);
    m_number->markThisObjectDontNeedStructureTransitionTable(state);
    m_number->setPrototype(state, m_functionPrototype);
    m_numberPrototype = m_objectPrototype;
    m_numberPrototype = new NumberObject(state, 0);
    m_numberPrototype->setPrototype(state, m_objectPrototype);
    m_number->setFunctionPrototype(state, m_numberPrototype);
    m_numberPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_number));

    m_numberPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinNumberToString, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_numberPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toFixed),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toFixed, builtinNumberToFixed, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    m_number->setFunctionPrototype(state, m_numberPrototype);

    ObjectPropertyDescriptor::PresentAttribute allFalsePresent = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent
                                                                                                              | ObjectPropertyDescriptor::NonEnumerablePresent
                                                                                                              | ObjectPropertyDescriptor::NonConfigurablePresent);
    const StaticStrings* strings = &state.context()->staticStrings();
    // $20.1.2.6 Number.MAX_SAFE_INTEGER
    m_number->defineOwnPropertyThrowsException(state, strings->MAX_SAFE_INTEGER, ObjectPropertyDescriptor(Value(9007199254740991.0), (ObjectPropertyDescriptor::PresentAttribute)allFalsePresent));
    // $20.1.2.7 Number.MAX_VALUE
    m_number->defineOwnPropertyThrowsException(state, strings->MAX_VALUE, ObjectPropertyDescriptor(Value(1.7976931348623157E+308), (ObjectPropertyDescriptor::PresentAttribute)allFalsePresent));
    // $20.1.2.8 Number.MIN_SAFE_INTEGER
    m_number->defineOwnPropertyThrowsException(state, strings->MIN_SAFE_INTEGER, ObjectPropertyDescriptor(Value(-9007199254740991.0), (ObjectPropertyDescriptor::PresentAttribute)allFalsePresent));
    // $20.1.2.9 Number.MIN_VALUE
    m_number->defineOwnPropertyThrowsException(state, strings->MIN_VALUE, ObjectPropertyDescriptor(Value(5E-324), allFalsePresent));
    // $20.1.2.10 Number.NaN
    m_number->defineOwnPropertyThrowsException(state, strings->NaN, ObjectPropertyDescriptor(Value(std::numeric_limits<double>::quiet_NaN()), allFalsePresent));
    // $20.1.2.11 Number.NEGATIVE_INFINITY
    m_number->defineOwnPropertyThrowsException(state, strings->NEGATIVE_INFINITY, ObjectPropertyDescriptor(Value(-std::numeric_limits<double>::infinity()), allFalsePresent));
    // $20.1.2.14 Number.POSITIVE_INFINITY
    m_number->defineOwnPropertyThrowsException(state, strings->POSITIVE_INFINITY, ObjectPropertyDescriptor(Value(std::numeric_limits<double>::infinity()), allFalsePresent));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Number),
                      ObjectPropertyDescriptor(m_number, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
