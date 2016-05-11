/* 
 * Copyright clickmeeting.com 
 * Wojtek Kosak <wkosak@gmail.com>
 */

#ifndef _NGX_RTMPT_PROXY_SESSION_H_INCLUDED_
#define _NGX_RTMPT_PROXY_SESSION_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

extern char ngx_rtmpt_proxy_intervals_def[];


typedef struct ngx_rtmpt_proxy_session_s {
  ngx_buf_t 			*buffer_http;
  ngx_buf_t 			*buffer_rtmp;
  int					sock;
  unsigned long long 	sequence;
  ngx_pool_t			*pool,*out_pool;
  ngx_log_t				*log;
  
  ngx_str_t				name;
  time_t				interval_check_time;
  u_char				interval_check_att;
  u_char				interval_check_count;
  
  u_char				interval_position;
  
  ngx_chain_t			*chain_from_http_request;
  
  ngx_chain_t			*chain_from_nginx;
  u_char 				*buf_pos;
  
  void 					*on_finish_send;
  ngx_http_request_t	*actual_request;
  
  ngx_event_t 			http_timer;
  
  struct ngx_rtmpt_proxy_session_s *next,*prev;
  
  ngx_chain_t			*to_rtmp;
  ngx_chain_t			*from_rtmp;
  ngx_connection_t      *connection;
  
  
} ngx_rtmpt_proxy_session_t;

typedef struct {
	char *buf;
	char *start;
	int n;
	void *next;
} buffer_t;

buffer_t *from_rtmp, *to_rtmp;


ngx_rtmpt_proxy_session_t *ngx_rtmpt_proxy_create_session(ngx_http_request_t *r);
ngx_rtmpt_proxy_session_t *ngx_rtmpt_proxy_get_session(ngx_str_t *id);
void ngx_rtmpt_proxy_destroy_session(ngx_rtmpt_proxy_session_t *session);
void ngx_event_maxconn_write_handler(ngx_event_t *ev);

#endif