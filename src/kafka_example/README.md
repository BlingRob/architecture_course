# Run example

1) Start kafka-broker  docker run -p 9092:9092 apache/kafka:4.3.0
1. Запуск кластера
docker-compose up -d

2. Проверка статуса
docker-compose ps

3. Создание топиков
chmod +x scripts/create-topics.sh
./scripts/create-topics.sh

4. Проверка через Kafdrop UI
open http://localhost:9000

5. Просмотр логов
docker-compose logs -f kafka-1

6. Остановка кластера
docker-compose down

7. Полная очистка (включая данные)
docker-compose down -v

# Clean docker

# 1. Остановить ВСЕ контейнеры
sudo docker stop $(sudo docker ps -aq) 2>/dev/null

# 2. Удалить ВСЕ контейнеры
sudo docker rm $(sudo docker ps -aq) 2>/dev/null

# 3. Удалить ВСЕ образы
sudo docker rmi $(sudo docker images -q) 2>/dev/null

# 4. Удалить ВСЕ тома
sudo docker volume rm $(sudo docker volume ls -q) 2>/dev/null

# 5. Удалить ВСЕ сети (кроме стандартных)
sudo docker network rm $(sudo docker network ls -q | grep -v "bridge\|host\|none") 2>/dev/null

# 6. Полная очистка системы
sudo docker system prune -a --volumes -f

# 7. Проверка, что все чисто
sudo docker ps -a
sudo docker images
sudo docker volume ls


# Architecture scheme

┌────────────────────────────────────────────────────────────┐
│                     Docker Network                         │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Kafka-1    │  │   Kafka-2    │  │   Kafka-3    │      │
│  │ (Controller  │  │   (Broker)   │  │   (Broker)   │      │
│  │  + Broker)   │  │              │  │              │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                 │                 │              │
│         └─────────────────┼─────────────────┘              │
│                           │                                │
│                    ┌──────▼──────┐                         │
│                    │   Kafdrop   │                         │
│                    │    (UI)     │                         │
│                    └─────────────┘                         │
│                                                            │
│  ┌──────────────┐  ┌──────────────┐                        │
│  │  PostgreSQL  │  │    Redis     │                        │
│  │   (State)    │  │   (Cache)    │                        │
│  └──────────────┘  └──────────────┘                        │
│                                                            │
└────────────────────────────────────────────────────────────┘
                           │
                    ┌──────▼──────┐
                    │ C++ Clients │
                    │ (Producers/ │
                    │  Consumers) │
                    └─────────────┘