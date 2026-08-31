package com.smartvest.command.data

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.smartvest.command.R
import com.smartvest.command.databinding.ItemEventHistoryBinding
import com.smartvest.command.util.ThemeManager

class EventHistoryAdapter(private val items: List<EventHistoryItem>) :
    RecyclerView.Adapter<EventHistoryAdapter.EventHistoryViewHolder>() {

    inner class EventHistoryViewHolder(val binding: ItemEventHistoryBinding) :
        RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): EventHistoryViewHolder {
        val binding = ItemEventHistoryBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return EventHistoryViewHolder(binding)
    }

    override fun onBindViewHolder(holder: EventHistoryViewHolder, position: Int) {
        val item = items[position]
        val context = holder.binding.root.context

        holder.binding.txtHistoryTime.text = item.time
        holder.binding.txtHistoryEvent.text = item.eventName
        holder.binding.txtHistoryMotionTemp.text = item.motionTempText
        ThemeManager.applyTextMain(
            holder.binding.txtHistoryTime,
            holder.binding.txtHistoryEvent,
            holder.binding.txtHistoryMotionTemp
        )

        if (item.isOffline) {
            holder.binding.txtHistoryStatus.text = "● " + context.getString(R.string.status_offline)
            holder.binding.txtHistoryStatus.setBackgroundResource(R.drawable.bg_status_pill_offline)
            holder.binding.txtHistoryStatus.setTextColor(context.getColor(R.color.accent_red))
        } else if (item.isSafe) {
            holder.binding.txtHistoryStatus.text = "✓ " + context.getString(R.string.status_safe)
            holder.binding.txtHistoryStatus.setBackgroundResource(R.drawable.bg_status_pill_online)
            holder.binding.txtHistoryStatus.setTextColor(context.getColor(R.color.accent_green))
        } else {
            holder.binding.txtHistoryStatus.text = "! " + context.getString(R.string.status_alert)
            holder.binding.txtHistoryStatus.setBackgroundResource(R.drawable.bg_status_pill_offline)
            holder.binding.txtHistoryStatus.setTextColor(context.getColor(R.color.accent_red))
        }
    }

    override fun getItemCount(): Int = items.size
}
