modded class SCR_VONController
{
	override void ComputeSpectatorLR(int playerId, out float outLeft, out float outRight, out int silencedDecibels = 0)
	{		
		if (IsInOurVONRoom(playerId) && !IsMuted(playerId))
		{
			outLeft = 1;
			outRight = 1;
			silencedDecibels = 0;
		}
		else 
		{
			outLeft = 0;
			outRight = 0;
			silencedDecibels = 0;
		}		
	}
	
	protected bool IsInOurVONRoom(int playerId)
	{
		return PS_VoNRoomsManager.GetInstance().GetPlayerRoom(playerId) == PS_VoNRoomsManager.GetInstance().GetPlayerRoom(SCR_PlayerController.GetLocalPlayerId());
	}
	
	protected bool IsMuted(int playerId)
	{
		SocialComponent socialComp = SocialComponent.Cast(GetGame().GetPlayerController().FindComponent(SocialComponent));
		return socialComp.IsMuted(playerId);
	}
	
	protected PS_GameModeCoop m_gameMode;
	protected PS_GameModeCoop getGameMode()
	{
		if (m_gameMode == null)
		{
			m_gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		}
		
		return m_gameMode;
	}
	
	protected bool isSpectating(int playerId)
	{
		if (playerId < 1)
			return false;
		
		// While in game we are required to check if the player has faction.
		// If the player doesn't have faction he is in spectator
		if (getGameMode().GetState() == SCR_EGameModeState.GAME)
		{
			SCR_Faction faction = SCR_Faction.Cast(SCR_FactionManager.SGetPlayerFaction(playerId));
			if (!faction)
				return true;
			
			return false;
		}
		
		// In any other state we can expect spectator behaviour
		return true;
	}
	
	override void EOnFixedFrame(IEntity owner, float timeSlice)
	{
		if (m_fWriteTeamspeakClientIdCooldown > 0)
			m_fWriteTeamspeakClientIdCooldown -= timeSlice;
		else
			m_fWriteTeamspeakClientIdCooldown = 0;
		//Just in case....
		if (!CVON_VONGameModeComponent.GetInstance())
			return;
		if (!m_PlayerController)
			m_PlayerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		
		if (!m_Player && m_PlayerController)
			m_Player = m_PlayerController.GetControlledEntity();
		
		if (!m_CharacterController)
			if (m_Player)
				m_CharacterController = SCR_CharacterControllerComponent.Cast(m_Player.FindComponent(SCR_CharacterControllerComponent));
		
		if (!m_PlayerRplComponent)
			if (m_Player)
				m_PlayerRplComponent = RplComponent.Cast(m_Player.FindComponent(RplComponent));
		
		if (!m_PlayerRplComponent || !m_CharacterController || !m_Player)
			return;
		
		m_Camera = m_CameraManager.CurrentCamera();
		if (!m_Camera)
			return;
		
		m_PlayerIdTemp.Clear();
		m_PlayerManager.GetPlayers(m_PlayerIdTemp);
		
		bool localInSpec = isSpectating(SCR_PlayerController.GetLocalPlayerId());
		
		//When a player disconnects, they are no longer in the players array, so it just leaves an empty container.
		//This removes that container as when they reconnect they will no longer be heard.
		//Also handles head height caching
		//Also sound updating for maximum optimizations
		if (m_fHeadCacheBuffer >= 0.2)
		{
			UpdateHeadCache();
			m_fHeadCacheBuffer = 0;
		}
		else
			m_fHeadCacheBuffer += timeSlice;
		foreach (int playerId, CVON_VONContainer container: m_PlayerController.m_aLocalEntries)
		{
			if (!m_PlayerIdTemp.Contains(playerId))
			{
				m_PlayerController.m_aLocalEntries.Remove(playerId);
				continue;
			}
		
			if (container.m_SoundSource)
			{
				int maxDistance = m_VONGameModeComponent.GetPlayerVolume(playerId);
				maxDistance *= maxDistance;
				container.m_iVolume = m_VONGameModeComponent.GetPlayerVolume(playerId);
				
				float distance = vector.DistanceSq(container.m_SoundSource.GetOrigin(), m_Camera.GetOrigin());
				if (distance < maxDistance || localInSpec)
					container.m_fDistanceToSender = distance;
				else
					container.m_fDistanceToSender = -1;
				container.m_iVolume = m_VONGameModeComponent.GetPlayerVolume(playerId);
			}
			
		}
		
		foreach (int playerId: m_PlayerIdTemp)
		{
			if (!m_Player)
				continue;
			
			if (playerId == m_PlayerController.GetPlayerId())
				continue;
			
			IEntity player = m_PlayerManager.GetPlayerControlledEntity(playerId);
			if (!player)
			{
				if (m_PlayerController.m_aLocalEntries.Contains(playerId))
				{
					//If this VON Transmission is radio, don't do shit
					
					if (m_PlayerController.m_aLocalEntries.Get(playerId).m_eVonType == CVON_EVONType.RADIO)
						continue;
					m_PlayerController.m_aLocalEntries.Remove(playerId);
					continue;
				}
				else
					continue;
			}
			
			if (isSpectating(playerId))
			{
				if (localInSpec && IsInOurVONRoom(playerId))
				{
					if (m_PlayerController.m_aLocalEntries.Contains(playerId))
					{
						m_PlayerController.m_aLocalEntries[playerId].m_bIsSpectator = true;
					}
					else
					{
						CVON_VONContainer container = new CVON_VONContainer();
						container.m_eVonType = CVON_EVONType.DIRECT;
						container.m_iVolume = m_VONGameModeComponent.GetPlayerVolume(playerId);
						container.m_SenderRplId = RplComponent.Cast(m_PlayerManager.GetPlayerControlledEntity(playerId).FindComponent(RplComponent)).Id();
						container.m_iClientId = m_PlayerController.GetPlayersTeamspeakClientId(playerId);
						container.m_iPlayerId = playerId;
						container.m_bIsSpectator = true;
						m_PlayerController.m_aLocalEntries.Insert(playerId, container);
					}
				}
				else 
				{
					// If target player in spec and we are not or they are not in our room, we can safely remove them from our list.
					if (m_PlayerController.m_aLocalEntries.Contains(playerId))
					{
						m_PlayerController.m_aLocalEntries.Remove(playerId);
					}
				}
				
				continue;
			}
			else
			{
				// Just ensures that if there was anyone marked as spectator they will be marked now non spectators
				if (m_PlayerController.m_aLocalEntries.Contains(playerId))
				{
					m_PlayerController.m_aLocalEntries[playerId].m_bIsSpectator = false;
				}
			}			
						
			SCR_CharacterControllerComponent charCont = SCR_CharacterControllerComponent.Cast(ChimeraCharacter.Cast(player).GetCharacterController());
			if (charCont.IsDead() || charCont.IsUnconscious())
				if (m_PlayerController.m_aLocalEntries.Contains(playerId))
				{
					m_PlayerController.m_aLocalEntries.Remove(playerId);
					continue;
				}
				else
					continue;
			
			int maxDistance = m_VONGameModeComponent.GetPlayerVolume(playerId);
			maxDistance *= maxDistance;
			float distance = vector.DistanceSq(player.GetOrigin(), m_Camera.GetOrigin());
			if (distance > maxDistance)
			{
				if (m_PlayerController.m_aLocalEntries.Contains(playerId))
				{
					//If this VON Transmission is radio, don't do shit
					if (m_PlayerController.m_aLocalEntries.Get(playerId).m_eVonType == CVON_EVONType.RADIO)
						continue;
					m_PlayerController.m_aLocalEntries.Remove(playerId);
					continue;
				}
				else
					continue;
			}
			else
			{
				if (m_PlayerController.m_aLocalEntries.Contains(playerId))
					continue;
				else
				{
					CVON_VONContainer container = new CVON_VONContainer();
					container.m_eVonType = CVON_EVONType.DIRECT;
					container.m_iVolume = m_VONGameModeComponent.GetPlayerVolume(playerId);
					container.m_SenderRplId = RplComponent.Cast(m_PlayerManager.GetPlayerControlledEntity(playerId).FindComponent(RplComponent)).Id();
					container.m_iClientId = m_PlayerController.GetPlayersTeamspeakClientId(playerId);
					container.m_iPlayerId = playerId;
					m_PlayerController.m_aLocalEntries.Insert(playerId, container);
				}
				
			}
		}
		
				
		//Handles broadcasting to other players
		if (m_bIsBroadcasting)
		{
			if (m_CharacterController.GetLifeState() != ECharacterLifeState.ALIVE)
			{
				if (m_bToggleBuffer)
				{
					m_bToggleBuffer = false;
					DeactivateCVON();
					m_VONHud.DirectToggleDelay();
				}
				else
					DeactivateCVON();
				return;
			}
			m_PlayerController.BroadcastLocalVONToServer(m_CurrentVONContainer, m_PlayerIdTemp, m_PlayerController.GetPlayerId(), m_CurrentVONContainer.m_iRadioId);
					
		}
		WriteJSON();
	}
}