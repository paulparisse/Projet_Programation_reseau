# Contrat de protocole

Ce document définit le protocole de communication applicatif commun aux clients et au serveur. Il s'applique à tous les jalons qui échangent des messages structurés.

L'implémentation de référence du squelette se trouve dans `travail/include/protocol.h` et `travail/src/common/protocol.c`.

## Modèle d'échange

Chaque trame contient :

1. un en-tête de taille fixe ;
2. un payload optionnel de `payload_len` octets.

Une opération de réception peut retourner une partie de l'en-tête, plusieurs trames, ou une partie du payload. Le programme doit donc utiliser des boucles de lecture et d'écriture, telles que `protocol_recv_all()` et `protocol_send_all()`, pour transmettre les tailles attendues.

## En-tête sur le réseau

L'en-tête est transmis champ par champ, dans cet ordre :

| Champ | Taille | Encodage | Règle |
| --- | ---: | --- | --- |
| `type` | 2 octets | entier non signé | Doit être un type de message défini. |
| `payload_len` | 4 octets | entier non signé | Doit être inférieur ou égal à `PROTO_MAX_PAYLOAD`. |
| `nick_sender` | 128 octets | texte UTF-8 ou ASCII, complété par des zéros | Doit contenir un octet nul dans les 128 octets. |
| `infos` | 128 octets | texte UTF-8 ou ASCII, complété par des zéros | Doit contenir un octet nul dans les 128 octets. |

La structure C `message_t` est une représentation locale. On supposera que vos programmes s'execute sur des machines de meme architecture de compilation.

## Payload et limites

- `payload_len == 0` signifie qu'aucun payload ne suit l'en-tête.
- La limite initiale est `PROTO_MAX_PAYLOAD = 65 536` octets par message applicatif.
- Un fichier volumineux doit être découpé en plusieurs messages applicatifs. Un message applicatif ne représente pas nécessairement un fichier complet.
- Avant d'allouer ou de lire un payload, le récepteur valide sa taille.

Cette limite est volontairement indépendante de la taille maximale d'un fichier. Elle évite qu'un pair malveillant ou défectueux puisse provoquer une allocation arbitrairement grande.

## Fermeture et erreurs

les clients doivent proprement fermer leurs connexions en envoyantle dernier message, puis en appelant `close(fd)`.

Les cas suivants sont des erreurs de protocole ou de transport et doivent etre traitées :
- fin de flot au milieu d'un en-tête ou d'un payload
- type de message inconnu
- longueur supérieure à `PROTO_MAX_PAYLOAD`
- chaîne non terminée par un '\0' dans `nick_sender` ou `infos`
- échec de `send()` ou `recv()` et de tout appel système.

## Types de messages

Les types initiaux sont `NICKNAME_NEW`, `NICKNAME_LIST`, `NICKNAME_INFOS`, `ECHO_SEND`, `UNICAST_SEND` et `BROADCAST_SEND`. Le jalon 3 introduit `FILE_REQUEST`, `FILE_ACCEPT`, `FILE_REJECT`, `FILE_SEND` et `FILE_ACK`. Le jalon 4, extension, introduit les types `MULTICAST_*`. Toute extension doit être documenter. 

## Cas de test minimaux

Chaque équipe doit vérifier le bon déroulement de l'application de messagerie dans les cas suivants:
1. un message applicatif reçue en plusieurs lectures,
2. Une valeur de payload invalide ou excessive,
3. la déconnexion d'un client pendant la réception d'un message applicatif.
