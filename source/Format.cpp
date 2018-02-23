/* Format.h
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Format.h"
#include "LocaleInfo.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

using namespace std;



string Format::Number(double value)
{
	if(!value)
		return "0";
	
	// Check what the power will be, after the value is rounded to five digits.
	int power = floor(log10(fabs(value)) - log10(.999995));
	if(power >= 15 || power <= -5)
	{
		// Fall back to scientific notation if the number is outside the range
		// we can format "nicely".
		ostringstream out;
		out.precision(3);
		out << value;
		return out.str();
	}
	
	string result;
	result.reserve(8);
	
	bool isNegative = (value < 0.);
	bool nonzero = false;
	
	if(power >= 6)
	{
		nonzero = true;
		int place = (power - 6) / 3;
		
		static const char suffix[3] = {'M', 'B', 'T'};
		static const double multiplier[3] = {1e-6, 1e-9, 1e-12};
		result += suffix[place];
		value *= multiplier[place];
		power %= 3;
	}
	
	// The number of digits to the left of the decimal is max(0, power + 1).
	// e.g. if power = 0, 10 > value >= 1.
	int left = max(0, power + 1);
	int right = max(0, 5 - left);
	if(nonzero)
		right = min(right, 3);
	nonzero |= !right;
	int rounded = round(fabs(value) * pow(10., right));
	int delimiterIndex = -1;
	
	// Special case: the value is close enough to a power of 10 that it rounds
	// up to one. There is now an extra digit on the left. (This should never
	// happen due to the rounding in the initial power calculation.)
	const char dec = LocaleInfo::GetDecimalPoint();
	const char sep = LocaleInfo::GetThousandsSep();
	if(pow(10., left) <= rounded)
		++left;
	if(left > 3)
		delimiterIndex = left - 3;
	
	do {
		int digit = rounded % 10;
		if(nonzero || digit)
		{
			result += static_cast<char>(digit + '0');
			nonzero = true;
		}
		rounded /= 10;
		
		if(right)
		{
			--right;
			if(!right)
			{
				if(nonzero)
				{
					result += dec;
					if(!rounded)
						result += '0';
				}
				nonzero = true;
			}
		}
		else
		{
			--left;
			if(left == delimiterIndex && rounded)
				result += sep;
		}
	} while(rounded || right);
	
	// Add the negative sign if needed.
	if(isNegative)
		result += '-';
	
	// Reverse the string.
	reverse(result.begin(), result.end());
	
	return result;
}



// Format the given value as a number with exactly the given number of
// decimal places (even if they are all 0).
string Format::Decimal(double value, int places)
{
	double integer;
	double fraction = fabs(modf(value, &integer));
	
	string result = to_string(static_cast<int>(integer)) + ".";
	while(places--)
	{
		fraction = modf(fraction * 10., &integer);
		result += ('0' + static_cast<int>(integer));
	}
	return result;
}



// Convert a string into a number. As with the output of Number(), the
// string can have suffixes like "M", "B", etc.
double Format::Parse(const string &str)
{
	double place = 1.;
	double value = 0.;
	
	string::const_iterator it = str.begin();
	string::const_iterator end = str.end();
	while(it != end && (*it < '0' || *it > '9') && *it != '.')
		++it;
	
	for( ; it != end; ++it)
	{
		if(*it == '.')
			place = .1;
		else if(*it < '0' || *it > '9')
			break;
		else
		{
			double digit = *it - '0';
			if(place < 1.)
			{
				value += digit * place;
				place *= .1;
			}
			else
			{
				value *= 10.;
				value += digit;
			}
		}
	}
	
	if(it != end)
	{
		if(*it == 'k' || *it == 'K')
			value *= 1e3;
		else if(*it == 'm' || *it == 'M')
			value *= 1e6;
		else if(*it == 'b' || *it == 'B')
			value *= 1e9;
		else if(*it == 't' || *it == 'T')
			value *= 1e12;
	}
	
	return value;
}



string Format::Replace(const string &source, const map<string, string> keys)
{
	string result;
	result.reserve(source.length());
	
	size_t start = 0;
	size_t search = start;
	while(search < source.length())
	{
		size_t left = source.find('<', search);
		if(left == string::npos)
			break;
		
		size_t right = source.find('>', left);
		if(right == string::npos)
			break;
		
		bool matched = false;
		++right;
		size_t length = right - left;
		for(const auto &it : keys)
			if(!source.compare(left, length, it.first))
			{
				result.append(source, start, left - start);
				result.append(it.second);
				start = right;
				search = start;
				matched = true;
				break;
			}
		
		if(!matched)
			search = left + 1;
	}
	
	result.append(source, start, source.length() - start);
	return result;
}



string Format::Capitalize(const string &str)
{
	string result = str;
	bool first = true;
	for(char &c : result)
	{
		if(!isalpha(c))
			first = true;
		else
		{
			if(first && islower(c))
				c = toupper(c);
			first = false;
		}
	}
	return result;
}



string Format::LowerCase(const string &str)
{
	string result = str;
	for(char &c : result)
		c = tolower(c);
	return result;
}



// Split a single string into substrings with the given separator.
vector<string> Format::Split(const string &str, const string &separator)
{
	vector<string> result;
	size_t begin = 0;
	while(true)
	{
		size_t pos = str.find(separator, begin);
		if(pos == string::npos)
			pos = str.length();
		result.emplace_back(str, begin, pos - begin);
		begin = pos + separator.size();
		if(begin >= str.length())
			break;
	}
	return result;
}



// Make a string according to a format-string.
string Format::StringF(initializer_list<string> args)
{
	if(args.size() == 1)
		return *args.begin();
	else if(args.size() > 1)
	{
		string buf;
		int state = 0;
		size_t n = 0;
		size_t pos = 0;
		for(const auto c : *args.begin())
		{
			buf += c;
			if(state == 0)
			{
				if(c == '%')
				{
					state = 1;
					n = 0;
					pos = buf.length() - 1;
				}
			}
			else
			{
				if(c == '%')
				{
					if(0 < n && n < args.size())
					{
						buf.erase(pos, buf.length());
						buf += args.begin()[n];
					}
					state = 0;
				}
				else if(isdigit(c))
					n = 10*n + c - '0';
				else
					state = 0;
			}
		}
		return buf;
	} else
		return "";
}



Format::ListOfWords::ListOfWords()
	: separators(1, vector<string>{ ", " })
{
}



Format::ListOfWords::ListOfWords(const string &format)
	: separators()
{
	SetSeparators(format);
}



void Format::ListOfWords::SetSeparators(const string &format)
{
	separators.clear();
	const size_t sz = format.length();
	size_t n = 2;
	size_t m = 0;
	const char delm = sz > 0 ? format[0] : 'x';
	string token;
	for(size_t i = 1; i < sz; ++i)
	{
		if(format[i] != delm)
			token += format[i];
		if(format[i] == delm || i == sz - 1)
		{
			if(m == 0)
				separators.emplace_back(vector<string>{ token });
			else
				separators.back().push_back(token);
			token.clear();
			++m;
			if(n == m + 1)
			{
				++n;
				m = 0;
			}
		}
	}
	if(m != 0)
		// Something wrong.
		separators.pop_back();
	if(separators.empty())
		separators.emplace_back(vector<string>{ ", " });
}



string Format::ListOfWords::GetList(size_t n, const function<string()> &GetAndNext)
{
	string s;
	if(n == 0)
		return s;
	else if (n == 1)
	{
		s = GetAndNext();
		return s;
	}
	else
	{
		const size_t m = separators.size() > n - 2 ? n - 2 : separators.size() - 1;
		const size_t sz = separators[m].size();
		const size_t firstHalf = (sz - 1) / 2;
		const size_t secondHalf = n - 1 - (sz - firstHalf);
		size_t j = 0;
		s = GetAndNext();
		for(size_t i = 0; i < n - 1; ++i)
		{
			s += separators[m][j];
			s += GetAndNext();
			if(i < firstHalf || secondHalf <= i)
				++j;
		}
		return s;
	}
}
