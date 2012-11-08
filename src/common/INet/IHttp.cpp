#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "Gdc2.h"
#include "IHttp.h"
#include "GToken.h"
#include "LgiCommon.h"
#include "INetTools.h"
#include "GString.h"
#include "Base64.h"
#include "GVariant.h"

///////////////////////////////////////////////////////////////////
class ILogProxy : public GSocketI
{
	GSocketI *Dest;

public:
	ILogProxy(GSocketI *dest)
	{
		Dest = dest;
	}

	void OnError(int ErrorCode, char *ErrorDescription)
	{
		if (Dest && ErrorDescription)
		{
			char Str[256];
			sprintf(Str, "[Data] %s", ErrorDescription);
			Dest->OnInformation(Str);
		}
	}

	void OnInformation(char *s)
	{
		if (Dest && s)
		{
			char Str[256];
			sprintf(Str, "[Data] %s", s);
			Dest->OnInformation(Str);
		}
	}

	void OnDisconnect()
	{
		if (Dest)
		{
			Dest->OnInformation("[Data] Disconnect");
		}
	}
};

///////////////////////////////////////////////////////////////////
IHttp::IHttp()
{
	Meter = 0;
	ResumeFrom = 0;
	Proxy = 0;
	ProxyPort = 0;
	FileLocation = 0;
	Headers = 0;
	AuthUser = 0;
	AuthPassword = 0;
	NoCache = false;
	BufferLen = 16 << 10;
	Buffer = new char[BufferLen];
}

IHttp::~IHttp()
{
	Close();
	DeleteArray(Proxy);
	DeleteArray(FileLocation);
	DeleteArray(Headers);
	DeleteArray(Buffer);
	DeleteArray(AuthUser);
	DeleteArray(AuthPassword);
}

void IHttp::SetProxy(char *p, int Port)
{
	DeleteArray(Proxy);
	Proxy = NewStr(p);
	ProxyPort = Port;
}

void IHttp::SetAuth(char *User, char *Pass)
{
	DeleteArray(AuthUser);
	DeleteArray(AuthPassword);
	
	AuthUser = NewStr(User);
	AuthPassword = NewStr(Pass);
}

bool IHttp::Open(GAutoPtr<GSocketI> S, char *RemoteHost, int Port)
{
	Close();
	Socket = S;

	if (Proxy)
	{
		RemoteHost = Proxy;
		Port = ProxyPort;
	}
	
	if (RemoteHost)
	{
		char Host[256];
		char *p = stristr(RemoteHost, "://");
		if (p)
		{
			char *s = Host;
			for (p += 3; *p && *p != ':' && *p != '/'; p++)
			{
				*s++ = *p;
			}
			*s++ = 0;

			if (*p == ':' && IsDigit(p[1]))
			{
				Port = atoi(p + 1);
			}
		}
		else
		{
			p = strchr(RemoteHost, '/');
			if (p)
			{
				int Len = p-RemoteHost;
				memcpy(Host, RemoteHost, Len);
				Host[Len] = 0;
			}
			else
			{
				strcpy(Host, RemoteHost);
			}
		}

		if (Socket &&
			Socket->Open(Host, Port > 0 ? Port : 80))
		{
			return true;
		}
	}

	return false;
}

bool IHttp::Close()
{
	if (Socket)
	{
		Socket->Close();
	}
	return 0;
}

bool IHttp::IsOpen()
{
	return Socket != 0;
}

#if 0
bool IHttp::GetFile(char *File, GStream &Out, int Format, int *ProtocolStatus, int *DataLength)
{
	bool Status = false;
	GVariant v;

	DeleteArray(FileLocation);
	DeleteArray(Headers);

	if (File && Socket)
	{
		GStringPipe Buf;
		// char *ContentStr = "Content-Length:";
		// char Line[4096] = "", *Cur = Line;
		// int ContentLength = 0;
		// int DataToGo = 1000000000;

		GUri u(File);
		if (!u.Protocol)
		{
			printf("%s:%i - No protocol.\n", _FL);
			return false;
		}

		// get part
		if (Format & GET_TYPE_NORMAL)
		{
			Buf.Print("GET %s HTTP/1.0\r\n", File);
		}
		else
		{
			Buf.Print(	"GET %s HTTP/1.0\r\n"
						"Host: %s\r\n",
						ValidStr(u.Path) ? u.Path : (char*)"/",
						u.Host);
		}
		if (Format & GET_NO_CACHE)
		{
			Buf.Print("Cache-Control: no-cache\r\n");
		}
		
		// resume info
		if (ResumeFrom > 0)
		{
			Buf.Print("Range:Bytes=%i-\r\n", ResumeFrom);
		}

		// cache override
		if (NoCache)
		{
			Buf.Print("Cache-Control:no-cache\r\n");
		}

		// end of request
		Buf.Print("\r\n");

		// write request
		GVariant v;
		Socket->SetValue(GSocket_Log, v = true);
		char *Str = Buf.NewStr();
		if (Str)
		{
			Socket->Write(Str, strlen(Str), 0);
			DeleteArray(Str);
		}
		
		// read headers
		int Code = 0, r = 0, HeaderSize = 0, Length = 0;
		GLinePrefix EndHeaders("\r\n");
		GStringPipe In;

		int Start = LgiCurrentTime();
		do
		{
			CheckReadable:
			if (Socket->IsReadable())
			{
				r = Socket->Read(Buffer, BufferLen, 0);
				if (r > 0)
				{
					In.Push(Buffer, r);
				}
				else
				{
					break;
				}
			}
			else
			{
				if (LgiCurrentTime() - Start > 30000)
				{
					HeaderSize = 0;
					break;
				}
				
				LgiSleep(500);
				goto CheckReadable;
			}
		}
		while ((HeaderSize = EndHeaders.IsEnd(Buffer, r)) < 0);

		Headers = HeaderSize ? new char[HeaderSize+1] : 0;
		if (Headers)
		{
			In.GBytePipe::Read((uchar*)Headers, HeaderSize);
			Headers[HeaderSize] = 0;

			GToken L(Headers, "\r\n");

			char *Response = L[0];
			if (Response)
			{
				GToken R(Response, " \t");
				if (R.Length() >= 2 &&
					strnicmp(R[0], "http/", 5) == 0)
				{
					Code = atoi(R[1]);
					if (ProtocolStatus)
					{
						*ProtocolStatus = Code;
					}
				}
			}

			if (Code == 200 ||
				Code == 206)
			{
				// Look for the content's length in the headers
				char *ContentLength = InetGetHeaderField(Headers, "Content-Length");
				if (ContentLength)
				{
					Length = atoi(ContentLength);
					DeleteArray(ContentLength);
				}

				// Output any overrun data after reading the headers
				int Size = In.GetSize();
				int Pos = Size;
				if (Size)
				{
					char *Data = (char*)In.New();
					if (Data)
					{
						Out.Write(Data, Size);
						DeleteArray(Data);
					}
				}

				// Start reading the rest of the document
				if (Length > 0)
				{
					Socket->SetValue(GSocket_TransferSize, v = Length);

					if (Meter &&
						Meter->Lock(_FL))
					{
						Meter->SetLimits(0, Length);
						Meter->Unlock();
					}

					for (int i=Pos; i<Length; )
					{
						int Len = min(BufferLen, Length - i);
						r = Socket->Read(Buffer, Len, 0);
						if (r > 0)
						{
							Out.Write(Buffer, r);
							i += r;
							Pos += r;

							if (Meter &&
								Meter->Lock(_FL))
							{
								Meter->Value(i);
								Meter->Unlock();
							}
						}
						else
						{
							break;
						}
					}

					Status = Pos == Length;
				}
				else
				{
					while (Socket->IsOpen())
					{
						r = Socket->Read(Buffer, BufferLen, 0);
						if (r > 0)
						{
							Out.Write(Buffer, r);
						}
						else
						{
							break;
						}
					}

					// We have no way of know whether we have all the document :(
					Status = true;
				}
			}
			else if (Code == 301 || Code == 302)
			{
				if (FileLocation = InetGetHeaderField(Headers, "Location"))
				{
					Status = true;
				}
			}
		}

		// Turn logging on
		Socket->SetValue(GSocket_Log, v = true);
	}
	else
	{
		printf("%s:%i - Invalid arguments.\n", __FILE__, __LINE__);
	}

	return Status;
}
#endif

enum HttpRequestType
{
	HttpNone,
	HttpGet,
	HttpPost
};

bool IHttp::Request
(
	const char *Type,
	char *Uri,
	int *ProtocolStatus,
	const char *InHeaders,
	GStreamI *InBody,
	GStreamI *Out,
	GStreamI *OutHeaders,
	ContentEncoding *OutEncoding
)
{
	// Input validation
	if (!Socket || !Uri || !Out || !Type)
		return false;

	HttpRequestType ReqType = HttpNone;
	if (!stricmp(Type, "GET"))
		ReqType = HttpGet;
	else if (!stricmp(Type, "POST"))
		ReqType = HttpPost;
	else
		return false;

	// Generate the request string
	GStringPipe Cmd;
	GUri u(Uri);
	bool IsHTTPS = u.Protocol && !stricmp(u.Protocol, "https");
	GAutoString EncPath = u.Encode(u.Path ? u.Path : (char*)"/"), Mem;
	char s[1024];
	GLinePrefix EndHeaders("\r\n");
	GStringPipe Headers;

	if (IsHTTPS && Proxy)
	{
		Cmd.Print(	"CONNECT %s:%i HTTP/1.1\r\n"
					"Host: %s\r\n"
					"\r\n",
					u.Host, u.Port ? u.Port : HTTPS_PORT,
					u.Host);
		GAutoString c(Cmd.NewStr());
		int cLen = strlen(c);
		int r = Socket->Write(c, cLen);
		if (r == cLen)
		{
			int Length = 0;
			while (Out)
			{
				int r = Socket ? Socket->Read(s, sizeof(s)) : -1;
				if (r > 0)
				{
					int e = EndHeaders.IsEnd(s, r);
					if (e < 0)
					{
						Headers.Write(s, r);
					}
					else
					{
						e -= Length;
						Headers.Write(s, e);
						break;
					}
					
					Length += r;
				}
				else break;
			}
			
			GAutoString Hdr(Headers.NewStr());

			GVariant v;
			if (Socket)
				Socket->SetValue(GSocket_Protocol, v = "SSL");
			
			EndHeaders.Reset();
		}
		else return false;
	}

	Cmd.Print("%s %s HTTP/1.1\r\n", Type, (Proxy && !IsHTTPS) ? Uri : EncPath.Get());
	Cmd.Print("Host: %s\r\n", u.Host);
	if (InHeaders) Cmd.Print("%s", InHeaders);

	if (AuthUser && AuthPassword)
	{
		if (1)
		{
			// Basic authentication
			char Raw[128];
			sprintf(Raw, "%s:%s", AuthUser, AuthPassword);
			char Base64[128];
			ZeroObj(Base64);
			ConvertBinaryToBase64(Base64, sizeof(Base64)-1, (uchar*)Raw, strlen(Raw));
			
			Cmd.Print("Authorization: Basic %s\r\n", Base64);
		}
		else
		{
			// Digest authentication
			// Not implemented yet...
			LgiAssert(!"Not impl.");
		}
	}
	Cmd.Push("\r\n");
	
	GAutoString c(Cmd.NewStr());
	bool Status = false;
	if (Socket && c)
	{
		// Write the headers...
		int cLen = strlen(c);
		bool WriteOk = Socket->Write(c, cLen) == cLen;
		c.Reset();
		if (WriteOk)
		{
			// Write any body...
			if (InBody)
			{
				int r;
				while ((r = InBody->Read(s, sizeof(s))) > 0)
				{
					int w = Socket->Write(s, r);
					if (w < r)
					{
						return false;
					}
				}
			}
			
			// Read the response
			int Total = 0;
			int Used = 0;
			
			while (Out)
			{
				int r = Socket ? Socket->Read(s, sizeof(s)) : -1;
				if (r > 0)
				{
					int e = EndHeaders.IsEnd(s, r);
					if (e < 0)
					{
						Headers.Write(s, r);
					}
					else
					{
						e -= Total;
						Headers.Write(s, e);
						if (r > e)
						{
							// Move the tailing data down to the start of the buffer
							memmove(s, s + e, r - e);
							Used = r - e;
						}
						break;
					}
					Total += r;
				}
				else break;
			}

			// Process output
			GAutoString h(Headers.NewStr());
			if (h)
			{
				GAutoString sContentLen(InetGetHeaderField(h, "Content-Length"));
				int64 ContentLen = sContentLen ? atoi64(sContentLen) : -1;
				bool IsChunked = false;
				if (ContentLen > 0)
					Out->SetSize(ContentLen);
				else
				{
					GAutoString sTransferEncoding(InetGetHeaderField(h, "Transfer-Encoding"));
					IsChunked = sTransferEncoding && !stricmp(sTransferEncoding, "chunked");
				}
				GAutoString sContentEncoding(InetGetHeaderField(h, "Content-Encoding"));
				ContentEncoding Encoding = EncodeRaw;
				if (sContentEncoding && !stricmp(sContentEncoding, "gzip"))
				{
					Encoding = EncodeGZip;
					if (OutEncoding) *OutEncoding = Encoding;
				}

				if (strnicmp(h, "HTTP/", 5) == 0 && ProtocolStatus)
				{
					*ProtocolStatus = atoi(h + 9);
				}					
				if (OutHeaders)
				{
					OutHeaders->Write(h, strlen(h));
				}

				int64 Written = 0;
				if (IsChunked)
				{
					while (true)
					{
						// Try and get chunk header
						char *End = strnstr(s, "\r\n", Used);
						if (!End)
						{
							int r = Socket->Read(s + Used, sizeof(s) - Used);
							if (r < 0)
								break;
							
							Used += r;
							
							End = strnstr(s, "\r\n", Used);
							if (!End)
							{
								LgiAssert(!"No chunk header");
								break;
							}
						}

						// Process the chunk header							
						End += 2;
						int HdrLen = End - s;
						int ChunkSize = htoi(s);
						if (ChunkSize <= 0)
						{
							// End of stream.
							Status = true;
							break;
						}
						
						int ChunkDone = 0;
						memmove(s, End, Used - HdrLen);
						Used -= HdrLen;
						
						// Loop over the body of the chunk
						while (Socket && ChunkDone < ChunkSize)
						{
							int Remaining = ChunkSize - ChunkDone;
							int Common = min(Used, Remaining);
							if (Common > 0)
							{
								int w = Out->Write(s, Common);
								if (w != Common)
								{
									LgiAssert(!"Write failed.");
									break;
								}
								if (Used > Common)
									memmove(s, s + Common, Used - Common);
								ChunkDone += Common;
								Used -= Common;

								if (ChunkDone >= ChunkSize)
									break;
							}

							int r = Socket->Read(s + Used, sizeof(s) - Used);
							if (r < 0)
								break;
							
							Used += r;
						}
						
						// Loop over the CRLF postfix
						if (Socket && Used < 2)
						{
							int r = Socket->Read(s + Used, sizeof(s) - Used);
							if (r < 0)
								break;
							Used += r;
						}							
						if (Used < 2 || s[0] != '\r' || s[1] != '\n')
						{
							LgiAssert(!"Post fix missing.");
							break;
						}
						if (Used > 2)
							memmove(s, s + 2, Used - 2);
						Used -= 2;							
					}
				}
				else
				{
					// Non chunked connection.
					int64 Written = 0;
					if (Used > 0)
					{
						int w = Out->Write(s, Used);
						if (w >= 0)
						{
							Written += w;
							Used = 0;
						}
						else
						{
							Written = -1;
						}
					}
					
					while (	Socket &&
							Written >= 0 &&
							Written < ContentLen &&
							Socket->IsOpen())
					{
						int r = Socket->Read(s + Used, sizeof(s) - Used);
						if (r <= 0)
							break;

						int w = Out->Write(s, r);
						if (w <= 0)
							break;
						
						Written += w;
					}

					Status = Written == ContentLen;
				}
			}
		}
	}

	return Status;
}

#if 0
#define HTTP_POST_LOG 1

bool IHttp::Post
(
	char *File,
	const char *ContentType,
	GStreamI *In,
	int *ProtocolStatus,
	GStreamI *Out,
	GStreamI *OutHeaders,
	char *InHeaders
)
{
	bool Status = false;

	if (Socket && File && Out)
	{
		// Read in the data part of the PUT command, because we need the
		// Content-Length to put in the headers.
		GStringPipe Content;
		while (In)
		{
			char s[1024];
			int r = In->Read(s, sizeof(s));
			if (r > 0)
			{
				Content.Write(s, r);
			}
			else break;
		}
		
		// Generate the request string
		GStringPipe Cmd;
		Cmd.Print("POST %s HTTP/1.0\r\n", File);
		if (InHeaders) Cmd.Print("%s", InHeaders);
		if (ContentType) Cmd.Print("Content-Type: %s\r\n", ContentType);
		Cmd.Print("Content-Length: %i\r\n\r\n", Content.GetSize());
		char *s = Cmd.NewStr();
		if (s)
		{
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post write headers:\n%s\n\n", s);
#endif

			// Write the headers...
			int SLen = strlen(s);
			bool WriteOk = Socket->Write(s, SLen) == SLen;
			DeleteArray(s);

#if HTTP_POST_LOG
LgiTrace("IHTTP::Post WriteOk=%i\n", WriteOk);
#endif
			if (WriteOk)
			{
				// Copy all the 'Content' to the socket
				char Buf[1024];
				while (true)
				{
					int r = Content.Read(Buf, sizeof(Buf));
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post Content.Read=%i\n", r);
#endif
					if (r > 0)
					{
						int w = Socket->Write(Buf, r);
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post Socket.Write=%i\n", w);
#endif
						if (w <= 0)
						{
							break;
						}
					}
					else break;
				}
				
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post Starting on response\n");
#endif
				// Read the response
				GLinePrefix EndHeaders("\r\n");
				int Total = 0;
				GStringPipe Headers;
				while (Out)
				{
					int r = Socket->Read(Buf, sizeof(Buf));
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post Read=%i\n", r);
#endif
					if (r > 0)
					{
						int e = EndHeaders.IsEnd(Buf, r);
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post EndHeaders=%i\n", e);
#endif
						if (e < 0)
						{
							Headers.Write(Buf, r);
						}
						else
						{
							e -= Total;
							Headers.Write(Buf, e);
							Out->Write(Buf + e, r - e);
							break;
						}
						Total += r;
					}
					else break;
				}
				
				// Process output
				char *h = Headers.NewStr();
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post Headers=\n%s\n\n", h);
#endif
				if (h)
				{
					if (ProtocolStatus)
					{
						char *Eol = strchr(h, '\r');
						if (Eol)
						{
							GToken Resp(h, " ", true, Eol-h);
							if (Resp.Length() > 1)
								*ProtocolStatus = atoi(Resp[1]);
						}
					}
					if (OutHeaders)
					{
						OutHeaders->Write(h, strlen(h));
					}
					DeleteArray(h);
					
					while (Socket->IsOpen())
					{
						int r = Socket->Read(Buf, sizeof(Buf));
#if HTTP_POST_LOG
LgiTrace("IHTTP::Post ResponseBody.Read=%i\n", r);
#endif
						if (r > 0)
						{
							Out->Write(Buf, r);
						}
						else break;
					}
					
					Status = true;
				}
			}
		}
	}
	
	return Status;
}
#endif