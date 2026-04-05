/*
For general Scribus copyright and licensing information please refer
to the COPYING file provided with the program. A GPL2+ license text follows.
*/
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "sctextcodec.h"

#include <QDebug>
#include <QVarLengthArray>

#include <unicode/ucnv.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

ScTextCodec::ScTextCodec(const char *codecName, Flags flags)
	: m_ignoreHeader(flags & IgnoreHeader)
{
	// Check if ICU actually knows this encoding
	bool found = false;
	int32_t count = ucnv_countAvailable();
	for (int32_t i = 0; i < count; ++i)
	{
		if (qstricmp(codecName, ucnv_getAvailableName(i)) == 0)
		{
			found = true;
			break;
		}
	}

	// ICU also recognises aliases, so try opening anyway
	// but warn that the canonical name wasn't found
	if (!found)
		m_lastError = QString("Codec '%1' not found in ICU available list, trying as alias").arg(QString::fromUtf8(codecName));

	UErrorCode err = U_ZERO_ERROR;
	m_converter = ucnv_open(codecName, &err);
	if (U_FAILURE(err))
	{
		m_lastError = QString("Failed to open codec '%1' - ICU error: %2").arg(QString::fromUtf8(codecName), QString::fromUtf8(u_errorName(err)));
		m_converter = nullptr;
		return;
	}

	// If we warned about the alias but it opened successfully, clear the error
	if (found)
		m_lastError.clear();
}

ScTextCodec::~ScTextCodec()
{
	if (m_converter)
		ucnv_close(m_converter);
}

ScTextCodec::ScTextCodec(ScTextCodec &&other) noexcept
	: m_converter(other.m_converter),
	  m_ignoreHeader(other.m_ignoreHeader)
{
	other.m_converter = nullptr;
}

ScTextCodec &ScTextCodec::operator=(ScTextCodec &&other) noexcept
{
	if (this != &other)
	{
		if (m_converter)
			ucnv_close(m_converter);
		m_converter = other.m_converter;
		m_ignoreHeader = other.m_ignoreHeader;
		m_lastError.clear();
		other.m_converter = nullptr;
	}
	return *this;
}

QString ScTextCodec::toUnicode(const QByteArray &data)
{
	return toUnicode(data.constData(), data.size());
}

QString ScTextCodec::toUnicode(const char *data, int length)
{
	if (!m_converter || !data || length <= 0)
		return {};

	UErrorCode err = U_ZERO_ERROR;

	// First attempt: assume output won't exceed input length in UChars
	int32_t bufSize = length + 1;
	QVarLengthArray<UChar, 1024> buf(bufSize);
	int32_t len = ucnv_toUChars(m_converter, buf.data(), bufSize, data, length, &err);

	// Handle buffer overflow by reallocating and retrying
	if (err == U_BUFFER_OVERFLOW_ERROR)
	{
		err = U_ZERO_ERROR;
		buf.resize(len + 1);
		ucnv_resetToUnicode(m_converter);
		len = ucnv_toUChars(m_converter, buf.data(), len + 1, data, length, &err);
	}

	if (U_FAILURE(err))
	{
		m_lastError = QString("toUnicode conversion failed - ICU error: %1").arg(QString::fromUtf8(u_errorName(err)));
		return {};
	}

	QString result(reinterpret_cast<const QChar *>(buf.data()), len);

	// Strip leading BOM (U+FEFF) unless IgnoreHeader was requested
	if (!m_ignoreHeader && !result.isEmpty() && result.at(0) == QChar(0xFEFF))
		result = result.mid(1);

	return result;
}

QByteArray ScTextCodec::fromUnicode(const QString &str)
{
	if (!m_converter || str.isEmpty())
		return {};

	UErrorCode err = U_ZERO_ERROR;
	const auto *src = reinterpret_cast<const UChar *>(str.constData());

	// Calculate maximum possible output size
	int32_t maxBytes = UCNV_GET_MAX_BYTES_FOR_STRING(str.size(), ucnv_getMaxCharSize(m_converter));
	QByteArray result(maxBytes, Qt::Uninitialized);

	int32_t len = ucnv_fromUChars(m_converter, result.data(), result.size(), src, str.size(), &err);

	if (U_FAILURE(err))
	{
		m_lastError = QString("fromUnicode conversion failed - ICU error: %1").arg(QString::fromUtf8(u_errorName(err)));
		return {};
	}

	result.resize(len);
	return result;
}

ScTextCodec ScTextCodec::codecForLocale()
{
	return ScTextCodec(ucnv_getDefaultName());
}

QByteArray ScTextCodec::transcode(const QByteArray &data, const char *fromCodec, const char *toCodec)
{
	ScTextCodec decoder(fromCodec);
	ScTextCodec encoder(toCodec);
	if (!decoder.isValid() || !encoder.isValid())
		return {};
	return encoder.fromUnicode(decoder.toUnicode(data));
}