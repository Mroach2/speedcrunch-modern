// SPDX-FileCopyrightText: 2015, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef RATIONAL_H
#define RATIONAL_H

#include <QHashFunctions>

class HNumber;
class QString;

class Rational
{
    int m_num;
    int m_denom;
    bool m_valid;

    inline int gcd(int a, int b) const {return (b==0) ? a : gcd(b, a%b);}
    void normalize();
    int compare(const Rational & other) const;

public:
    Rational() : m_num(0), m_denom(1), m_valid(true) {}
    Rational(const HNumber &num);
    Rational(const double &num);
    Rational(const QString & str);
    static bool approximate(const HNumber& num, int maxDenominator,
        const HNumber& relativeTolerance, Rational* out);
    Rational(const int a, const int b) : m_num(a), m_denom(b), m_valid(true) {normalize();}
    Rational(const Rational& other) = default;

    int numerator() const {return m_num;}
    int denominator() const {return m_denom;}

    Rational& operator=(const Rational& other) = default;
    Rational& operator=(Rational&& other) noexcept = default;
    Rational operator*(const Rational & other) const;
    Rational operator/(const Rational & other) const;
    Rational operator+(const Rational & other) const;
    Rational operator-(const Rational & other) const;
    Rational &operator+=(const Rational & other);
    Rational &operator-=(const Rational & other);
    Rational &operator*=(const Rational & other);
    Rational &operator/=(const Rational & other);
    bool operator<(const Rational & other) const;
    bool operator==(const Rational & other) const;
    bool operator!=(const Rational & other) const {return !operator==(other);}
    bool operator>(const Rational & other) const;

    bool isZero() const;
    bool isValid() const;

    QString toString() const;
    HNumber toHNumber( )const;
    double toDouble() const;
};

inline size_t qHash(const Rational& value, size_t seed = 0) noexcept
{
    return qHashMulti(seed, value.numerator(), value.denominator());
}

#endif // RATIONAL_H
