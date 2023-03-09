#pragma once

#include "n_common.h"
#include "n_message.h"

namespace BAO_Net 
{

	template<typename T>
	class connection : public std::enable_shared_from_this<connection<T>>
	{
	public:

		enum class owner
		{
			server,
			client
		};

	protected:

		uint32_t ID = NULL;
		asio::ip::tcp::socket socket;
		asio::io_context& context;
		owner ownerType = owner::server;

		// incomming messages
		BAO_Ds::lock_free_queue<owned_message>& messagesIn;

		// outcomming messages
		BAO_Ds::lock_free_queue<message<T>> messagesOut;

		message<T> tmpMsgIn;

		// hsGuard has to be linked with encryption function for safety measurements
		uint64_t hsGuard = NULL;
		uint64_t hsOut = NULL;
		uint64_t hsIn = NULL;

	private:

		void SetHeader()
		{
			asio::async_write(socket, asio::buffer(&messagesOut.front().header, sizeof(message_header<T>)),
				[this](std::error_code errc, std::size_t length)
				{
					if (!errc)
					{
						if (messagesOut.front().body().size() > 0)
						{
							SetBody();
						}
						else
						{
							messagesOut.pop();
							if (messagesOut.empty())
							{
								SetHeader();
							}
						}
					}
					else
					{
						BAOWARN("ID: " + std::to_string(ID) + " SetHeader fail");
						socket.close();
					}
				})
		}

		void SetBody()
		{
			asio::async_write(socket, asio::buffer(messagesOut.front().body().data(), 
				messagesOut.front().body().size()), 
				[this](std::error_code errc, std::size_t length)
				{
					if (!errc)
					{
						messagesOut.pop();
						if (messagesOut.empty())
						{
							SetHeader();
						}
					}
					else
					{
						BAOWARN("ID: " + std::to_string(ID) + " SetBody fail");
						socket.close();
					}
				})
		}

		void GetHeader()
		{
			asio::async_read(socket, asio::buffer(&tmpMsgIn.header, sizeof(msg_header<T>)),
				[this](std::error_code errc, std::size_t length)
				{
					if (!errc)
					{
						if (tmpMsgIn.header.size > 0)
						{
							tmpMsgIn.body.resize(tmpMsgIn.header.size);
							GetBody();
						}
						else
						{
							SlideToInMessages()
						}
					}
					else
					{
						BAOWARN("ID: " + std::to_string(ID) + " GetHeader fail");
						socket.close();
					}
				});
		}

		void GetBody()
		{
			asio::async_read(socket, asio::buffer(tmpMsgIn.body.data(), tmpMsgIn.body().size()),
				[this](std::error_code errc, std::size_t length)
				{
					if (!errc)
					{
						SlideToInMessages();
					}
					else
					{
						BAOWARN("ID: " + std::to_string(ID) + " GetBody fail");
						socket.close();
					}
				}
		}

		void SlideToInMessages()
		{
			if (ownerType == owner::server)
				messagesIn.push({ this->shared_from_this(), tmpMsgIn });
			else
				messagesIn.push({ nullptr, tmpMsgIn });

			GetHeader();
		}

	public:

		connection(const owner parent, asio::io_context& _context, asio::ip::tcp::socket _socket, BAO_Ds::lock_free_queue<owned_message<T>>& in)
			: context(_context), socket(std::move(_socket)), messagesIn(in)
		{
			ownerType = parent;

			if (ownerType == owner::server)
			{
				hsOut = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
			}
			else
			{
				hsOut = NULL;
				hsIn = NULL;
			}
		}

		virtual ~connection()
		{}

		uint32_t GetID() const
		{
			return ID;
		}

		void ConnectToClient(uint32_t _ID = 0)
		{
			if (ownerType == owner::server)
			{
				if (socket.is_open())
				{
					ID = _ID;
					GetHeader();
				}
			}
		}

		void ConnectToServer(const asio::ip::tcp::resolver::results_type& endps)
		{
			if (ownerType == owner::client)
			{
				asio::async_connect(socket, endps,
					[this](std::error_code errc, asio::ip::tcp::endpoint endp)
					{
						if (!errc)
						{
							GetHeader();
						}
					});
			}
		}

		bool Disconnect()
		{
			if (IsConnected())
			{
				asio::post(context,
					[this]()
					{
						socket.close();
					});
			}
		}

		bool IsConnected() const
		{
			return socket.is_open();
		}

		bool SendMsg(const message<T>& msg)
		{
			asio::post(context,
				[this, msg]()
				{
					bool isItBusy = !messagesOut.empty();
					messagesOut.push(msg);

					if (!isItBusy)
					{
						SetHeader();
					}
				});
		}
	};
}