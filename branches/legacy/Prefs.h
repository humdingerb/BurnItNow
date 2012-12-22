/*
 * Copyright 2000-2002, Johan Nilsson. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _PREFS_H_
#define _PREFS_H_


#include <Message.h>
#include <Path.h>


class Prefs : public BMessage {
public:
	Prefs(const char* filename);
	~Prefs();
	status_t		InitCheck(void);

	status_t		SetBool(const char* name, bool b);
	status_t		SetRef(const char* name, const entry_ref* ref);
	status_t		SetInt8(const char* name, int8 i);
	status_t		SetInt16(const char* name, int16 i);
	status_t		SetInt32(const char* name, int32 i);
	status_t		SetInt64(const char* name, int64 i);
	status_t		SetFloat(const char* name, float f);
	status_t		SetDouble(const char* name, double d);
	status_t		SetString(const char* name, const char* string);
	status_t		SetPoint(const char* name, BPoint p);
	status_t		SetRect(const char* name, BRect r);
	status_t		SetMessage(const char* name, const BMessage* message);
	status_t		SetFlat(const char* name, const BFlattenable* obj);

private:
	BPath			fPath;
	status_t		fStatus;
};


inline status_t Prefs::InitCheck(void) {
	return fStatus;
}


#endif	// _PREFS_H_
