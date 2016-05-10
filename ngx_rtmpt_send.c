/* 
 * Copyright clickmeeting.com 
 * Wojtek Kosak <wkosak@gmail.com>
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>

#include "ngx_rtmpt_proxy_session.h"
#include "ngx_rtmpt_proxy_module.h"
#include "ngx_rtmpt_send.h"


void ngx_rtmpt_read_from_rtmp(ngx_event_t *ev) {
	ngx_connection_t           			*c;
	ngx_rtmpt_proxy_session_t           *s;
	int 								n;
	ngx_http_request_t					*r;
	int									first_chain_created = 0;
	
	unsigned char 						buffer[8192];
	
	
	c = ev->data;
	s = c->data;
	r = s->actual_request;
	
	
	if (c->destroyed) {
		return;
	}
	
	ngx_log_debug(NGX_LOG_DEBUG, s->log, 0,"rtmpt/read: enter into read function");
	
	while (true) {	
		n = c->recv(c, buffer, 8192);

	
		ngx_log_debug1(NGX_LOG_DEBUG, s->log, 0, "rtmpt/read: received %i bytes", n);
	
		if (n>0) {
		
			ngx_chain_t	*chain, *new_chain;
			if (!s->out_pool) {
				s->out_pool = ngx_create_pool(4096, s->log);
				first_chain_created = 1;
			}
			new_chain = ngx_alloc_chain_link(s->out_pool);
			new_chain->next = NULL;
			
			if (!new_chain) {
				ngx_log_error(NGX_LOG_ERR, s->log, 0, "rtmpt/read: cannot allocate chain");
				ngx_rtmpt_proxy_destroy_session(s);
				return;
			}
		
			if (first_chain_created) {
				chain = s->chain_from_nginx = new_chain;
			} else {
				
				chain=s->chain_from_nginx;
				while (true) {
					if (!chain->next) break;
					chain = chain->next;
				}
				chain->next = new_chain;
				chain = chain->next;
			}
		

			chain->buf = ngx_create_temp_buf(s->out_pool, n + first_chain_created);
			if (!chain->buf) {
				ngx_log_error(NGX_LOG_ERR, s->log, 0, "rtmpt/read: cannot allocate %i bytes for buffer in chain",n + first_chain_created);
				ngx_rtmpt_proxy_destroy_session(s);
				return;
			}
			
			if (first_chain_created) {
				char one[1];
				one[0]=1;			
				chain->buf->last = ngx_cpymem(chain->buf->last, one, 1);
			}
		
			chain->buf->last = ngx_cpymem(chain->buf->last, buffer, n);
	
		} else {
			if (n == NGX_ERROR || n == 0) {
				ngx_log_error(NGX_LOG_ERR, s->log, 0, "rtmpt/read: error during read - finealizing session");
				ngx_rtmpt_proxy_destroy_session(s);
				return;
			}
			
			break;
		}
		
	}

}


void ngx_rtmpt_send_chain_to_rtmp(ngx_event_t *wev) {
	ngx_connection_t           	    *c;
	ngx_rtmpt_proxy_session_t       *s;
	int 							n;
	FILE *f;
	int sent=0;
	int size=0;
	ngx_chain_t *ch;

	c = wev->data;
	s = c->data;


	ngx_log_debug0(NGX_LOG_DEBUG, s->log, NGX_ETIMEDOUT, "rtmpt/send: enter into write function");

	if (wev->active) {
	    ngx_del_event(wev, NGX_WRITE_EVENT, 0);
	}

	if (!s->on_finish_send) {
		return;
	}

	if (c->destroyed) {
		return;
	}
	
	
	if (wev->timedout) {
		ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "rtmpt/send: rtmp server timed out");
		c->timedout = 1;
		ngx_rtmpt_proxy_destroy_session(s);
		return;
	}

	
	while (s->chain_from_http_request) {
		n = c->send(c, s->buf_pos, s->chain_from_http_request->buf->last - s->buf_pos);


		if (n == NGX_AGAIN || n == 0) {
			ngx_add_timer(c->write, 1000);//s->timeout);
			
			if (ngx_handle_write_event(c->write, 0) != NGX_OK) {
				ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "rtmpt/send: adding event failed");
				ngx_rtmpt_proxy_destroy_session(s);
			}
			
			return;
		} else if (n < 0) {
			ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "rtmpt/send: error during sending");
			ngx_rtmpt_proxy_destroy_session(s);
			return;
		}
	
		s->buf_pos+=n;
		
		if (s->buf_pos == s->chain_from_http_request->buf->last) {
			
			s->chain_from_http_request = s->chain_from_http_request->next;
			
			if (s->chain_from_http_request && s->chain_from_http_request->buf) {	
				s->buf_pos = s->chain_from_http_request->buf->pos;
			}	
		}	
	}
	
	
	if (s->on_finish_send) {
		ngx_rtmpt_handler_pt handler=s->on_finish_send;
		handler(s);
	}	
}