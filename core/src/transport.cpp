#include "irods/private/http_api/transport.hpp"

#include "irods/private/http_api/globals.hpp"

#include "irods/private/http_api/log.hpp"

#include <stdexcept>

namespace logging = irods::http::log;

namespace irods::http
{
	transport::transport(boost::asio::io_context& _ctx)
		: io_ctx_{_ctx}
	{
		logging::trace("{}: Building transport...", __func__);
	}

	auto transport::connect(std::string_view _host, std::string_view _port) -> void
	{
		logging::trace("{}: Connecting to {}:{}...", __func__, _host, _port);
		auto res{resolve(_host, _port)};
		do_connect(res);
		did_connect_ = true;
		logging::trace("{}: Connected.", __func__);
	}

	auto transport::is_connected() const noexcept -> bool
	{
		return did_connect_;
	}

	auto transport::communicate(boost::beast::http::request<boost::beast::http::string_body>& _request)
		-> boost::beast::http::response<boost::beast::http::string_body>
	{
		logging::trace("{}: Sending data...", __func__);
		do_write(_request);
		logging::trace("{}: Receiving data...", __func__);
		return do_read();
	}

	auto transport::resolve(std::string_view _host, std::string_view _port)
		-> boost::asio::ip::tcp::resolver::results_type
	{
		logging::trace("{}: Resolving {}:{}...", __func__, _host, _port);
		boost::asio::ip::tcp::resolver tcp_res{io_ctx_};
		return tcp_res.resolve(_host, _port);
	}

	tls_transport::tls_transport(boost::asio::io_context& _ctx, boost::asio::ssl::context& _secure_ctx)
		: transport{_ctx}
		, stream_{_ctx, _secure_ctx}
	{
	}

	tls_transport::~tls_transport()
	{
		logging::trace("{}: Deconstructing tls transport...", __func__);
		if (is_connected()) {
			logging::trace("{}: Disconnecting from remote host...", __func__);
			disconnect();
		}
	}

	auto tls_transport::resolve(std::string_view _host, std::string_view _port)
		-> boost::asio::ip::tcp::resolver::results_type
	{
		set_sni_hostname(_host);
		return transport::resolve(_host, _port);
	}

	auto tls_transport::do_connect(boost::asio::ip::tcp::resolver::results_type& _resolved_host) -> void
	{
		boost::beast::get_lowest_layer(stream_).connect(_resolved_host);
		stream_.handshake(boost::asio::ssl::stream_base::client);
	}

	auto tls_transport::do_write(boost::beast::http::request<boost::beast::http::string_body>& _request) -> void
	{
		boost::beast::http::write(stream_, _request);
	}

	auto tls_transport::do_read() -> boost::beast::http::response<boost::beast::http::string_body>
	{
		boost::beast::flat_buffer buffer;
		boost::beast::http::response<boost::beast::http::string_body> res;
		boost::beast::http::read(stream_, buffer, res);

		return res;
	}

	auto tls_transport::disconnect() -> void
	{
		boost::beast::error_code ec;
		stream_.shutdown(ec);
		if (ec) {
			if (ec != boost::asio::ssl::error::stream_truncated) {
				logging::error("{}: disconnect error, what=[{}]", __func__, ec.what());
			}
			else {
				logging::trace("{}: disconnected, message=[{}]", __func__, ec.message());
			}
		}
	}

	auto tls_transport::set_sni_hostname(std::string_view _host) -> void
	{
		logging::trace("{}: Setting SSL hostname...", __func__);
		// Set SNI Hostname (many hosts need this to handshake successfully)
		if (!SSL_set_tlsext_host_name(stream_.native_handle(), _host.data())) {
			boost::beast::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
			throw boost::beast::system_error{ec};
		}
	}

	plain_transport::plain_transport(boost::asio::io_context& _ctx)
		: transport{_ctx}
		, stream_{_ctx}
	{
	}

	plain_transport::~plain_transport()
	{
		if (is_connected()) {
			disconnect();
		}
	}

	auto plain_transport::do_connect(boost::asio::ip::tcp::resolver::results_type& _resolved_host) -> void
	{
		stream_.connect(_resolved_host);
	}

	auto plain_transport::do_write(boost::beast::http::request<boost::beast::http::string_body>& _request) -> void
	{
		boost::beast::http::write(stream_, _request);
	}

	auto plain_transport::do_read() -> boost::beast::http::response<boost::beast::http::string_body>
	{
		boost::beast::flat_buffer buffer;
		boost::beast::http::response<boost::beast::http::string_body> res;
		boost::beast::http::read(stream_, buffer, res);

		return res;
	}

	auto plain_transport::disconnect() -> void
	{
		boost::beast::error_code ec;
		stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

		if (ec) {
			if (ec != boost::asio::ssl::error::stream_truncated) {
				logging::error("{}: disconnect error, what=[{}]", __func__, ec.what());
			}
			else {
				logging::trace("{}: disconnected, message=[{}]", __func__, ec.message());
			}
		}
	}

	auto make_secure_context() -> boost::asio::ssl::context
	{
		boost::asio::ssl::context ctx{boost::asio::ssl::context::tlsv12_client};
		const auto config{irods::http::globals::oidc_configuration()};

		const std::string& cert_path{config.at("tls_certificates_directory").get_ref<const std::string&>()};
		ctx.add_verify_path(cert_path);
		ctx.set_verify_mode(boost::asio::ssl::verify_peer);

		return ctx;
	}

	auto transport_factory(const boost::urls::scheme& _scheme, boost::asio::io_context& _ctx)
		-> std::unique_ptr<transport>
	{
		if (_scheme == boost::urls::scheme::http) {
			logging::trace("{}: Creating Plain Transport...", __func__);
			return std::make_unique<plain_transport>(_ctx);
		}
		if (_scheme == boost::urls::scheme::https) {
			logging::trace("{}: Creating TLS Transport...", __func__);
			auto secure_context{make_secure_context()};
			return std::make_unique<tls_transport>(_ctx, secure_context);
		}
		throw std::invalid_argument{"Scheme is not a supported."};
	}
} //namespace irods::http
