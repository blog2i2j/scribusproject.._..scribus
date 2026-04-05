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

#ifndef SCTEXTCODEC_H
#define SCTEXTCODEC_H

#include <QString>
#include <QByteArray>

#include <unicode/ucnv.h>

/**
 * @brief A lightweight text encoding converter wrapping ICU's ucnv API.
 *
 * This class replaces QTextCodec in Qt5 Compatiblity code and
 * provides encoding and decoding between character encodings
 * and Unicode (QString). It supports all encodings provided by ICU,
 * including legacy encodings such as cp1252, KOI8-R, Shift-JIS, etc.
 *
 * ScTextCodec is non-copyable but movable.
 */
class ScTextCodec
{
	public:
		enum Flag
		{
			DefaultConversion = 0,
			IgnoreHeader      = 1  //!< Don't strip a leading BOM
		};
		Q_DECLARE_FLAGS(Flags, Flag)

		/**
		 * @brief Construct a codec for the given encoding name.
		 *
		 * The name can be a canonical ICU converter name or an alias
		 * (e.g., "UTF-8", "cp1252", "windows-1252", "Shift-JIS").
		 * Check isValid() after construction to verify the codec was
		 * successfully created.
		 *
		 * @param codecName The encoding name to open.
		 */
		explicit ScTextCodec(const char *codecName, Flags flags = DefaultConversion);

		~ScTextCodec();

		// Non-copyable
		ScTextCodec(const ScTextCodec &) = delete;
		ScTextCodec &operator=(const ScTextCodec &) = delete;

		// Movable
		ScTextCodec(ScTextCodec &&other) noexcept;
		ScTextCodec &operator=(ScTextCodec &&other) noexcept;

		/**
		 * @brief Returns true if the codec was successfully initialised.
		 */
		bool isValid() const { return m_converter != nullptr; }

		/**
		 * @brief Returns the error message from the last failed operation.
		 *
		 * This is cleared at the start of each toUnicode() or fromUnicode()
		 * call, so it always reflects the most recent operation. For use with
		 * qWarning/Debug or Document Log.
		 *
		 * @return The error message, or an empty string if no error occurred.
		 */
		QString lastError() const { return m_lastError; }

		/**
		 * @brief Decode a byte array from this encoding to Unicode.
		 *
		 * @param data The encoded byte array.
		 * @return The decoded QString, or an empty QString on failure.
		 */
		QString toUnicode(const QByteArray &data);

		/**
		 * @brief Decode raw bytes from this encoding to Unicode.
		 *
		 * This overload is useful for chunk-based decoding where you
		 * have a pointer and length rather than a QByteArray.
		 *
		 * @param data Pointer to the encoded data.
		 * @param length Number of bytes to decode.
		 * @return The decoded QString, or an empty QString on failure.
		 */
		QString toUnicode(const char *data, int length);

		/**
		 * @brief Encode a Unicode string to this encoding.
		 *
		 * @param str The Unicode string to encode.
		 * @return The encoded QByteArray, or an empty QByteArray on failure.
		 */
		QByteArray fromUnicode(const QString &str);

		/**
		 * @brief Create a codec for the system's default locale encoding.
		 *
		 * On Linux this will typically be UTF-8. On Windows it
		 * returns the system's ANSI codepage (e.g., windows-1252).
		 * This is the equivalent of QTextCodec::codecForLocale().
		 *
		 * @return A ScTextCodec for the locale encoding.
		 */
		static ScTextCodec codecForLocale();

		/**
		 * @brief Transcode a byte array directly from one encoding to another.
		 *
		 * This is a convenience method that decodes from the source encoding
		 * to Unicode, then re-encodes to the target encoding. Useful for
		 * one-shot conversions such as BOM-detected UTF-16 to UTF-8.
		 *
		 * @param data The source data.
		 * @param fromCodec The source encoding name.
		 * @param toCodec The target encoding name.
		 * @return The transcoded data, or an empty QByteArray on failure.
		 */
		static QByteArray transcode(const QByteArray &data, const char *fromCodec, const char *toCodec);

	private:
		UConverter *m_converter { nullptr };
		bool m_ignoreHeader { false };
		QString m_lastError;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ScTextCodec::Flags)

#endif // SCTEXTCODEC_H