#pragma once

#include "n_common.h"
#include "n_message.h"
#include "n_connection.h"

namespace BAO_Net
{
	template<typename T>
	class IServer
	{
	protected:

		const uint32_t ID = 3000;

		// incomming messages
		BAO_Ds::lock_free_queue<owned_message<T>> messagesIn;

		// validated connections
		std::deque<std::shared_ptr<connection<T>>> connections;

		asio::io_context context;
		std::thread contextThread;
		asio::ip::tcp::acceptor acceptor;

	public:

		IServer(uint16_t port) : acceptor(context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
		{

		}

		virtual ~IServer()
		{
			Stop();
		}

		// PER-UPDATE function
		void Update(size_t maxCount = -1, bool wait = false)
		{
			if (wait)
				messagesIn.wait_block();

			size_t count = NULL;

			while (count < maxCount)
			{
				auto tmp = messagesIn.pop();
				MessageReceived(tmp.remote, tmp.msg);
				count++;
			}
		}

		void WaitConnection()
		{
			acceptor.async_accept(
				[this](std::error_code errc, asio::ip::tcp::socket socket)
				{
					if (!errc)
					{
						BAOINFO("New Connection: " + std::to_string(socket.remote_endpoint()));

						auto tmp = std::make_shared<connection<T>>(connection<T>::owner::server,
							context, std::move(socket), messagesIn);

						if (ClientConnected(tmp))
						{
							connections.push_back(std::move(tmp));
							connections.back()->ConnectToClient(ID++);
							BAOINFO("connection " + std::to_string(connections.back()->GetID()) + " was approoved");
						}
						else
						{
							BAOWARN("connection wasn't approoved");
						}
					}
					else
					{
						BAOFATAL(errc.message());
					}

					WaitConnection();
				});
		}

		void MessageClient(const message<T>& msg, std::shared_ptr<connection<T>> client)
		{
			if (client && client->IsConnected())
			{
				client->Send(msg);
				BAOINFO("message was sent");
			}
			else
			{
				ClientDisconnected(client);
				client.clear();
				connections.erase(std::remove(connections.begin(), connections.end(), client), connections.end());
			}
		}

		void MessageAll(const message<T>& msg)
		{
			bool doInvalidClientsExist = false;

			for (auto& client : connections)
			{
				if (client && client->IsConnected())
				{
					// client connected
					client->Send(msg);
				}
				else
				{
					// client disconnected
					ClientDisconnected(client);
					client.clear();
					doInvalidClientsExist = true;
				}
			}

			if (doInvalidClientsExist)
			{
				connections.erase(std::remove(connections.begin(), connections.end()))
			}
		}

		bool Start()
		{
			try
			{
				WaitConnection();
				context = std::thread(
					[this]() 
					{ 
						context.run(); 
					});
			}
			catch (std::exception& ecx)
			{
				BAOFATAL("SERVER_FATAL " + ecx.what());
				return false;
			}
		}

		void Stop()
		{
			context.stop();
			if (contextThread.joinable)
				contextThread.join();
		}

	protected:

		virtual void MessageReceived(message<T>& msg, std::shared_ptr<connection<T>> client)
		{
			// currently blank
		}

		// auto-called when a client is connected
		virtual bool ClientConnected(std::shared_ptr<connection<T>> client)
		{
			BAOINFO("Client was connected");
		}

		// auto-called when the client is disconnected
		virtual void ClientDisconnected(std::shared_ptr<connection<T>> client)
		{
			BAOINFO("Removing a client");
		}
	};
}