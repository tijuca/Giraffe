%module inetmapi

%{
#include <mapix.h>
#include <mapidefs.h>
#include "options.h"
#include "inetmapi.h"
%}

%include "std_string.i"
%include "cstring.i"
%include "typemap.i"

%cstring_output_allocate(char** lppbuf, free(*$1));

/* Finalize output parameters */
%typemap(in,numinputs=0) (std::string *) (std::string temp) {
	$1 = &temp;
}
%typemap(argout) (std::string *) {
	/* @todo fix this not to go through a cstring */
	%append_output(SWIG_FromCharPtrAndSize($1->c_str(), $1->length()));
}

typedef struct _so {
        char *alternate_boundary;               // Specifies a specific boundary prefix to use when creating MIME boundaries
        bool no_recipients_workaround;  // Specified that we wish to accepts messages with no recipients (for example, when converting an attached email with no recipients)
        bool msg_in_msg;
        bool headers_only;
        bool add_received_date;
        bool force_tnef;
        bool force_utf8;
        char *charset_upgrade;
        bool allow_send_to_everyone;
        %extend {
			sending_options() {
				sending_options *sopt = new sending_options;
				imopt_default_sending_options(sopt);
				sopt->charset_upgrade = strdup(sopt->charset_upgrade); /* avoid free problems */
				return sopt;
			}
			~sending_options() {
				free(self->alternate_boundary);
				free(self->charset_upgrade);
			}
		}

} sending_options;

typedef struct _do {
        bool use_received_date;         // Use the 'received' date instead of the current date as delivery date
        bool mark_as_read;              // Deliver the message 'read' instead of unread
        bool add_imap_data;				// Save IMAP optimized data to the server
        /* LPSBinary user_entryid;         // If not NULL, specifies the entryid of the user for whom we are delivering. If set, allows generating PR_MESSAGE_*_ME properties. */

        %extend {
            delivery_options() { delivery_options *dopt = new delivery_options; imopt_default_delivery_options(dopt); return dopt; }
        }

} delivery_options;

HRESULT IMToMAPI(IMAPISession *lpSession, IMsgStore *lpMsgStore, IAddrBook *lpAddrBook, IMessage *lpMessage, const std::string &input, delivery_options dopt);
HRESULT IMToINet(IMAPISession *lpSession, IAddrBook *lpAddrBook, IMessage *lpMessage, char** lppbuf, sending_options sopt);

HRESULT createIMAPProperties(const std::string &input, std::string *lpEnvelope, std::string *lpBody, std::string *lpBodyStructure);
