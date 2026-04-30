// SPDX-FileCopyrightText: 2015-2016, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "rational.h"
#include "math/hmath.h"
#include "numberformatter.h"

#include <cmath>
#include <QString>
#include <QStringList>
#include <limits.h>
#include <limits>


void Rational::normalize()
{
    if(m_denom==0) {
        m_valid = false;
        return;
    }
    if(m_num==0) {
        m_denom=1;
        return;
    }
    int g = gcd(abs(m_num), abs(m_denom));
    m_num /= g;
    m_denom /= g;
    if(m_denom<0) {
        m_num = -m_num;
        m_denom = -m_denom;
    }
}

int Rational::compare(const Rational &other) const
{
    return m_num*other.m_denom - m_denom*other.m_num;
}

bool Rational::approximate(const HNumber& num, int maxDenominator,
    const HNumber& relativeTolerance, Rational* out)
{
    if (out == nullptr || maxDenominator < 1 || relativeTolerance <= HNumber(0) || num.isNan())
        return false;

    if (num.isZero()) {
        *out = Rational(0, 1);
        return true;
    }

    const HNumber nearIntTol("1e-18");
    const HNumber absNum = HMath::abs(num);

    auto withinTolerance = [&](const Rational& candidate) {
        const HNumber diff = HMath::abs(candidate.toHNumber() - num);
        const HNumber scale = HMath::abs(num);
        if (scale.isZero())
            return diff <= relativeTolerance;
        return diff <= relativeTolerance * scale;
    };

    long long pPrevPrev = 0;
    long long qPrevPrev = 1;
    long long pPrev = 1;
    long long qPrev = 0;
    HNumber val(absNum);

    while (true) {
        HNumber aNum = HMath::floor(val);
        HNumber frac = val - aNum;

        // Guard against rounding noise near integer boundaries.
        if (HMath::abs(HNumber(1) - frac) < nearIntTol) {
            aNum += 1;
            frac = 0;
        }

        if (aNum > HNumber(INT_MAX))
            break;
        const long long a = static_cast<long long>(aNum.toInt());

        if ((a != 0 && pPrev > (std::numeric_limits<long long>::max() - pPrevPrev) / a)
                || (a != 0 && qPrev > (std::numeric_limits<long long>::max() - qPrevPrev) / a)) {
            break;
        }

        const long long p = pPrevPrev + a * pPrev;
        const long long q = qPrevPrev + a * qPrev;
        if (q > maxDenominator)
            break;
        if (p > INT_MAX)
            break;

        Rational candidate(static_cast<int>(p), static_cast<int>(q));
        if (num.isNegative())
            candidate = Rational(-candidate.numerator(), candidate.denominator());

        if (withinTolerance(candidate)) {
            *out = candidate;
            return true;
        }

        if (HMath::abs(frac) < nearIntTol)
            break;

        pPrevPrev = pPrev;
        qPrevPrev = qPrev;
        pPrev = p;
        qPrev = q;

        val = HNumber(1) / frac;
    }

    return false;
}


/*
 *  Find a rational approximation to num using a continued fraction scheme.
 *  Code adapted from the 'Fraction' module for the PYTHON programming language,
 *  authored by Sjoerd Mullender and Jeffrey Yasskin.
 */
Rational::Rational(const HNumber &num) :
    m_num(1), m_denom(1), m_valid(1)
{
    const HNumber nearIntTol("1e-18");

    if(num.isZero()) {
        m_num = 0;
        return;
    }
    if(HMath::abs(num)>HNumber(INT_MAX) || HMath::abs(num)<HNumber(1)/HNumber(INT_MAX)) {
        m_valid = false;
        return;
    }
    if(num.isInteger()) {
        m_num = num.toInt();
        return;
    }
    const unsigned long long MAXD = INT_MAX/2; // maximal denominator
    unsigned long long p0=0, q0=1, p1=1, q1=0;
    HNumber val(HMath::abs(num));
    while(true) {
        HNumber aNum = HMath::floor(val);
        HNumber frac = val - aNum;

        // Guard against rounding noise near integer boundaries.
        if (HMath::abs(HNumber(1) - frac) < nearIntTol) {
            aNum += 1;
            frac = 0;
        }

        long a = aNum.toInt();
        unsigned long long q2 = q0 + a*q1;
        if(q2>MAXD)
            break;
        unsigned long long temp1=p0, temp2=p1, temp3=q1;
        p0 = temp2;
        q0 = temp3;
        p1 = temp1 + a*temp2;
        q1 = q2;
        if(HMath::abs(frac) < nearIntTol) break;
        val = HNumber(1)/frac;
        if(val>HNumber(MAXD)) break;
    }

    Rational bound(p1, q1);
    if(num<0) {
        bound.m_num *= -1;
    }
    *this = bound;

}

Rational::Rational(const double &num):
    m_num(1), m_denom(1), m_valid(1)
{
    if (num==0) {
       m_num = 0;
       return;
    }
    if (std::abs(num)>INT_MAX || std::abs(1./num)>INT_MAX) {
        m_valid = false;
        return;
    }
    const long long MAXD = INT_MAX/2; // maximal denominator
    long long p0=0, q0=1, p1=1, q1=0;

    double val = fabs(num);
    while(true) {
        unsigned long long  a = static_cast<unsigned long long>(floor(val));
        unsigned long long q2 = q0 + a*q1;
        if(q2>MAXD)
            break;
        unsigned long long temp1=p0, temp2=p1, temp3=q1;
        p0 = temp2;
        q0 = temp3;
        p1 = temp1 + a*temp2;
        q1 = q2;
        if(val==a) break;
        val = 1/(val-a);
    }

    Rational bound(p1, q1);
    if(num<0)
        bound.m_num *= -1;

    *this = bound;
}

Rational::Rational(const QString &str) : m_num(0), m_denom(1), m_valid(true)
{
    if(str=="") {
        m_valid=false;
        return;
    }
    QStringList l = str.split("/");
    if(l.size()==1) {
        bool ok;
        m_num = l.at(0).toInt(&ok);
        if(!ok) {
            m_valid=false;
            return;
        }
    } else if(l.size()==2) {
        bool ok;
        m_num = l.at(0).toInt(&ok);
        if(!ok) {
            m_valid=false;
            return;
        }
        m_denom = l.at(1).toInt(&ok);
        if(!ok) {
            m_valid=false;
            return;
        }
    }
}

Rational Rational::operator*(const Rational &other) const
{
    return Rational(this->m_num * other.m_num, this->m_denom * other.m_denom);
}

Rational Rational::operator/(const Rational &other) const
{
    if(other.isZero()) return Rational(1,0); // Rational(1,0) will set m_valid=false
    return Rational(this->m_num / other.m_num, this->m_denom / other.m_denom);
}

Rational Rational::operator+(const Rational &other) const
{
    return Rational(this->m_num*other.m_denom + this->m_denom*other.m_num, this->m_denom*other.m_denom);
}

Rational Rational::operator-(const Rational &other) const
{
    return Rational(this->m_num*other.m_denom - this->m_denom*other.m_num, this->m_denom*other.m_denom);
}

/*Rational &Rational::operator=(const Rational &other)
{
    m_num = other.m_num;
    m_denom = other.m_denom;
    m_valid = other.m_valid;
    return *this;
}*/

Rational &Rational::operator+=(const Rational &other)
{
    return operator=(*this + other);
}

Rational &Rational::operator-=(const Rational &other)
{
    return operator=(*this - other);
}

Rational &Rational::operator*=(const Rational &other)
{
    return operator=(*this * other);
}

Rational &Rational::operator/=(const Rational &other)
{
    return operator=(*this / other);
}

bool Rational::operator<(const Rational &other) const
{
    return compare(other)<0;
}

bool Rational::operator==(const Rational &other) const
{
    return compare(other) == 0;
}

bool Rational::operator>(const Rational &other) const
{
    return compare(other)>0;
}

bool Rational::isZero() const
{
    return m_num==0;
}

bool Rational::isValid() const
{
    return m_valid;
}

QString Rational::toString() const
{
    if(m_denom==1)
        return QString::fromLatin1("%1").arg(m_num);
    return QString::fromLatin1("%1/%2").arg(m_num).arg(m_denom);
}

HNumber Rational::toHNumber() const
{
    HNumber num(m_num);
    HNumber denom(m_denom);
    return num/denom;

}

double Rational::toDouble() const
{
    return double(m_num)/m_denom;
}
