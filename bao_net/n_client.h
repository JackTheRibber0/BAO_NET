#pragma once

#include "n_connection.h"
#include "n_message.h"
#include "n_common.h"

namespace BAO_Net 
{

	template<typename T>
	class client
	{
	private:
		BAO_Ds::lock_free_queue<owned_message<T>> messagesIn;

	protected:

		asio::io_context context;
		asio::ip::tcp::socket socket;
		std::thread contextThread;
		std::unique_ptr<connection<T>> connection;

	public:

		client() : socket(context)
		{

		}

		virtual ~client()
		{
			Disconnect();
		}

		bool Connect(const std::string& host, const uint16_t port) 
		{
			try
			{
				connection = std::make_unique<connection<T>>();
				asio::ip::tcp::resolver resolver(context);
				auto endpoints = resolver.resolve(host, std::to_string(port));

				connection = std::make_unique<connection<T>>(
					connection<T>::owner::client,
					context,
					asio::ip::tcp::socket(context), messagesIn);

				connection->ConnectToServer(endpoints);
				contextThread = std::thread([this]() { context.run(); });
			}
			catch (std::exception& exc)
			{
				BAOERROR("Client exception: " + ecx.what());
				return false;
			}
		}

		void Disconnect()
		{
			if (IsConnected())
				connection->Disconnect();

			context.stop();
			if (contextThread.joinable())
				contextThread.join();

			connection.release();
		}

		bool IsConnected() const
		{
			if (connection)
				return connection.IsConnected();

			return false;
		}

		BAO_Ds::lock_free_queue<owned_message<T>>& GetQueue()
		{
			return messagesIn;
		}
	};
}