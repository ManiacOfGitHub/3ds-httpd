#include "httpserver.h"

static void			compute_path(http_request *request)
{
	// Default value
	request->path = "/";
	char *request_type = get_request_name(request->type);
	char *path = request->header;

	// Unknown request type?
	if (request_type)
		path += strlen(request_type) + 1;
	path = strtok(path, " ");

	// If path isn't NULL
	if (path)
		request->path = path;
}

void				send_response(s32 client_id, http_response *response)
{
	char	header[256];
	memset(header, 0, 256);
	sprintf(header, HTTP_HEADER_TEMPLATE, response->code, get_http_code_name(response->code));

	// HTTP/1.1 code message
	send(client_id, header, strlen(header), 0);
	// Headers (TODO: Implement a global list?)
	send(client_id, response->content_type, strlen(response->content_type), 0);
	send(client_id, "Server: 3ds-httpd\r\n", 19, 0);
	// End the header section
	send(client_id, "\r\n", 2, 0);

	// Send the payload
	send(client_id, response->payload, strlen(response->payload), 0);
}

void				manage_connection(server *data, char *payload)
{
	http_request *request = memalloc(sizeof(http_request));
	request->payload = payload;
	request->header = strtok(payload, "\r\n");
	request->type = get_type(request->header);

	// Scan others lines
	char *rawData = strtok(NULL, "\r\n");
	while (rawData)
	{
		if (startWith(rawData, "Host: "))
			request->hostname = rawData + 6;
		else if (startWith(rawData, "User-Agent: "))
			request->agent = rawData + 12;
		rawData = strtok(NULL, "\r\n");
	}
	// extract the path from the header
	compute_path(request);

	// get request handler
	http_request_handler	*handler = get_request_handler(request);
	http_response			*response = NULL;

	// Use the handler to try to the response to send
	if (handler)
		response = handler->get_response(request);

	// If the response is null, we use the default "Not Implemented" page (returning a 501 error code)
	if (response == NULL)
	{
		// copy default response
		response = memalloc(sizeof(DEFAULT_PAGE));
		memcpy(response, &DEFAULT_PAGE, sizeof(DEFAULT_PAGE));
		response->content_type = (char*)memdup(response->content_type, strlen(response->content_type));
		response->payload = (char*)memdup(response->payload, strlen(response->payload));
	}

	printf("[%d]: %s %s (client: %s)\n", response->code, get_request_name(request->type), request->path, inet_ntoa(data->client_addr.sin_addr));
	send_response(data->client_id, response);

	// Clean all data created in this function because we don't need them anymore
	memdel((void**)&response->content_type);
	memdel((void**)&response->payload);
	memdel((void**)&response);
	memdel((void**)&request);
}
